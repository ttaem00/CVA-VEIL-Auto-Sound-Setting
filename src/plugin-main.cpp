#include <obs-module.h>
#include <obs-frontend-api.h>
#include <plugin-support.h>

#include "loudness-dock.h"
#include "preview-category-capture.h"

#include <QByteArray>
#include <QMainWindow>
#include <QDockWidget>
#include <QScreen>
#include <QTimer>

#include <util/config-file.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

static const char *DOCK_ID = "loudness-balance-monitor-dock";

static lbm::LoudnessDock *g_dock = nullptr;

static void on_frontend_event(enum obs_frontend_event event, void *private_data)
{
	UNUSED_PARAMETER(private_data);

	if (g_dock &&
	    (event == OBS_FRONTEND_EVENT_STREAMING_STARTING || event == OBS_FRONTEND_EVENT_RECORDING_STARTING ||
	     event == OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTING ||
	     event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING || event == OBS_FRONTEND_EVENT_PROFILE_CHANGING ||
	     event == OBS_FRONTEND_EVENT_SCRIPTING_SHUTDOWN || event == OBS_FRONTEND_EVENT_EXIT)) {
		g_dock->interrupt_preview();
	}
	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
		// Create dock after OBS UI is ready
		auto *main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
		if (main_window) {
			g_dock = new lbm::LoudnessDock();
			g_dock->setObjectName(DOCK_ID);

			if (!obs_frontend_add_dock_by_id(DOCK_ID, obs_module_text("DockTitle"), g_dock)) {
				obs_log(LOG_ERROR, "Failed to add dock");
				delete g_dock;
				g_dock = nullptr;
			} else {
				obs_log(LOG_INFO, "Dock registered successfully");

				// OBS restores DockState before this event, then hides newly added docks.
				config_t *user_config = obs_frontend_get_user_config();
				if (user_config) {
					const char *saved_state =
						config_get_string(user_config, "BasicWindow", "DockState");
					if (saved_state && *saved_state) {
						QByteArray dock_state = QByteArray::fromBase64(QByteArray(saved_state));
						if (main_window->restoreState(dock_state)) {
							obs_log(LOG_INFO, "Restored OBS dock state after registration");
						} else {
							obs_log(LOG_WARNING,
								"Failed to restore OBS dock state after registration");
						}
					}
				}
				// TRUEITEM 2026-11-09: an old floating dock can retain a very short
				// size. Give the main check controls a usable initial viewport.
				QTimer::singleShot(0, g_dock, [main_window] {
					auto *dock = main_window->findChild<QDockWidget *>(DOCK_ID);
					if (dock && dock->isFloating() && dock->height() < 650) {
						const QRect space = dock->screen()->availableGeometry();
						dock->resize(640, qMin(900, space.height() - 60));
						dock->move(qMax(space.left(), qMin(dock->x(), space.right()-dock->width())),
							   qMax(space.top(), qMin(dock->y(), space.bottom()-dock->height())));
					}
				});
			}
		}
	} else if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP && g_dock) {
		g_dock->on_scene_collection_cleanup();
	} else if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED && g_dock) {
		g_dock->on_scene_collection_changed();
	}
}

extern "C" bool obs_module_load(void)
{
	lbm::register_preview_category_capture();
	obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);

	obs_frontend_add_event_callback(on_frontend_event, nullptr);

	return true;
}

extern "C" void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(on_frontend_event, nullptr);

	// Note: OBS will delete the dock widget automatically
	g_dock = nullptr;

	obs_log(LOG_INFO, "plugin unloaded");
}
