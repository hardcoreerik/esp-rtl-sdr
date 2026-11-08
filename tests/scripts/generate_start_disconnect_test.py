"""Exercise actual driver start admission, commit, failure and disconnect cleanup."""
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[2]
source = Path(sys.argv[2]).read_text() if len(sys.argv) > 2 else (root / "src/esp_rtl_sdr.cpp").read_text()
template = (root / "tests/host/start_disconnect_harness.cpp.in").read_text()
def function(signature):
    """Extract one top-level C++ function using the driver source brace style."""
    start = source.index(signature)
    return source[start:source.index("\n}", start) + 2]
if "class StartInFlight {" in source:
    start = source.index("class StartInFlight {")
    scope = source[start:source.index("\n};", start) + 3]
    disconnect = function("static bool complete_pending_disconnect(")
else:
    start = source.index("        if (h->device_gone) {", source.index("static void client_task_fn("))
    body = source[start:source.index("        uint8_t addr = 0;", start)]
    disconnect = ("static bool complete_pending_disconnect(esp_rtl_sdr_handle *h) {\n"
                  + body.rsplit("        }", 1)[0]
                  + "            return true;\n        }\n return false;\n}\n")
    scope = ""
start_fn = function("esp_err_t esp_rtl_sdr_start(")
admission = start_fn[start_fn.index("    HandleLock lk(handle);"):start_fn.index("    esp_err_t ret = ESP_OK;")]
commit_start = start_fn.index("        esp_rtl_sdr_event_cb_t cb = nullptr;", start_fn.index("        handle->streaming = true;"))
commit = start_fn[commit_start:start_fn.index("    } while (0);", commit_start)]
failure = start_fn[start_fn.index("    (void)stop_stream_internal(handle, 1000);"):start_fn.rfind("\n}")]
for marker, code in [
    ("// ACTUAL_START_SCOPE", scope),
    ("// ACTUAL_DISCONNECT", disconnect),
    ("// ACTUAL_START_ADMISSION", admission),
    ("// ACTUAL_START_COMMIT", commit),
    ("// ACTUAL_START_FAILURE", failure),
]:
    template = template.replace(marker, code)
Path(sys.argv[1]).write_text(template)
