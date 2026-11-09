from pathlib import Path, PurePosixPath
import concurrent.futures, hashlib, json, urllib.request, zipfile

BASE = Path(__file__).resolve().parent
CACHE = BASE/'cache'
DEST = BASE/'portable-msvc'
CACHE.mkdir(exist_ok=True)
DEST.mkdir(exist_ok=True)
catalog = json.loads((BASE/'vs-catalog.json').read_bytes())
ids = {
    'Microsoft.VC.14.51.Tools.HostX64.TargetX64.base',
    'Microsoft.VC.14.51.Tools.HostX64.TargetX64.Res.base',
    'Microsoft.VC.14.51.CRT.Headers.base',
    'Microsoft.VC.14.51.CRT.Headers.Resources.base',
    'Microsoft.VC.14.51.CRT.x64.Desktop.base',
    'Microsoft.VC.14.51.CRT.x64.OneCore.Desktop.base',
    'Microsoft.VC.14.51.CRT.Redist.x64.OneCore.Desktop.base',
}
payloads = [x for p in catalog['packages'] if p['id'] in ids and p.get('language') in (None,'en-US') for x in p.get('payloads',[])]
sdk_version = '10.0.26100.9457'
SDK_SHA256 = {'microsoft.windows.sdk.cpp': '1c8b332abba402808880ddba4d70e663f834ded44cf4771a2c191e9485e7cdcd', 'microsoft.windows.sdk.cpp.x64': '52bd17c63d3b6d268f385c358f34d0eb595b9c1deb3273855abd2ec5c095d132'}
for package in ('microsoft.windows.sdk.cpp','microsoft.windows.sdk.cpp.x64'):
    payloads.append({'fileName':package+'.nupkg','url':f'https://api.nuget.org/v3-flatcontainer/{package}/{sdk_version}/{package}.{sdk_version}.nupkg','nuget':True,'sha256':SDK_SHA256[package]})

def download(p):
    target=CACHE/p['fileName']
    if not target.exists():
        with urllib.request.urlopen(p['url'],timeout=90) as r, target.open('wb') as f:
            while b:=r.read(1024*1024): f.write(b)
    digest=hashlib.sha256(target.read_bytes()).hexdigest()
    if 'sha256' in p:
        assert digest.lower()==p['sha256'].lower(),target
    print(f'downloaded {target.name} {target.stat().st_size//1048576} MiB',flush=True)
    return p,target,digest

results=[]
with concurrent.futures.ThreadPoolExecutor(max_workers=5) as pool:
    for p,target,digest in pool.map(download,payloads):
        results.append({'file':target.name,'sha256':digest,'url':p['url']})
        out=BASE/'sdk' if p.get('nuget') else DEST
        out.mkdir(exist_ok=True)
        with zipfile.ZipFile(target) as z:
            for x in z.infolist():
                name=x.filename.replace('\\','/')
                if not p.get('nuget'):
                    if not name.startswith('Contents/'):continue
                    name=name[len('Contents/'):]
                if not name or x.is_dir():continue
                parts=PurePosixPath(name).parts
                if '..' in parts or PurePosixPath(name).is_absolute():raise ValueError(name)
                path=(out/Path(*parts)).resolve()
                assert path.is_relative_to(out.resolve())
                path.parent.mkdir(parents=True,exist_ok=True)
                data=z.read(x)
                if path.exists() and path.read_bytes()==data:continue
                path.write_bytes(data)
(BASE/'toolchain-downloads.json').write_text(json.dumps(results,indent=2)+'\n',encoding='utf-8')
print('Portable payloads extracted',flush=True)
