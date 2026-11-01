"""Generate a host test of the installed SDK's actual USB interrupt decoder.
No SDK modification. Generate stock and upstream-patched variants for MSVC/GCC.
Usage: python check_usb_hal_error_order.py SDK_ROOT OUTPUT_DIR
Then cmake -S OUTPUT_DIR -B OUTPUT_DIR/build; cmake --build ... --config Debug
"""
import hashlib, json, re, sys
from pathlib import Path
sdk, out = map(Path, sys.argv[1:3]); out.mkdir(parents=True, exist_ok=True)
source_path = sdk / "components/hal/usb_dwc_hal.c"
source = source_path.read_text()
match = re.search(r"usb_dwc_hal_chan_event_t usb_dwc_hal_chan_decode_intr\([^{}]*\)\s*\{", source)
assert match
start = match.start(); depth = 1; end = match.end()
while depth:
    depth += (source[end] == "{") - (source[end] == "}"); end += 1
function = source[start:end]
line = "        HAL_ASSERT(chan_intrs & USB_DWC_LL_INTR_CHAN_CHHLTD);  //An error should have halted the channel\n"
assert function.count(line) == 1, "SDK differs: review before adapting"
stock = function.replace("usb_dwc_hal_chan_decode_intr(", "stock_decode(")
patched = function.replace(line, "").replace("usb_dwc_hal_chan_decode_intr(", "patched_decode(")
preamble = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#define HAL_ASSERT(x) do { if (!(x)) throw std::runtime_error("HAL assertion"); } while(0)
constexpr uint32_t USB_DWC_LL_INTR_CHAN_STALL=1, USB_DWC_LL_INTR_CHAN_BBLEER=2,
 USB_DWC_LL_INTR_CHAN_BNAINTR=4, USB_DWC_LL_INTR_CHAN_XCS_XACT_ERR=8,
 USB_DWC_LL_INTR_CHAN_CHHLTD=16, USB_DWC_LL_INTR_CHAN_XFERCOMPL=32;
constexpr uint32_t CHAN_INTRS_ERROR_MSK=15;
enum usb_dwc_hal_chan_error_t { USB_DWC_HAL_CHAN_ERROR_STALL,
 USB_DWC_HAL_CHAN_ERROR_PKT_BBL, USB_DWC_HAL_CHAN_ERROR_BNA, USB_DWC_HAL_CHAN_ERROR_XCS_XACT };
enum usb_dwc_hal_chan_event_t { USB_DWC_HAL_CHAN_EVENT_ERROR,
 USB_DWC_HAL_CHAN_EVENT_HALT_REQ, USB_DWC_HAL_CHAN_EVENT_CPLT, USB_DWC_HAL_CHAN_EVENT_NONE };
struct Registers { uint32_t bits; bool disabled=false; };
struct usb_dwc_hal_chan_t { Registers* regs; struct { bool active=true; bool halt_requested=false; } flags;
 usb_dwc_hal_chan_error_t error=USB_DWC_HAL_CHAN_ERROR_STALL; };
uint32_t usb_dwc_ll_hcint_read_and_clear_intrs(Registers* r) { auto b=r->bits; r->bits=0; return b; }
void usb_dwc_ll_hcchar_disable_chan(Registers* r) { r->disabled=true; }
unsigned checks=0;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); return 1; } } while(0)
"""
tests = r"""
int main() {
 for (uint32_t mask=1;mask<=15;++mask) {
  Registers r{mask}; usb_dwc_hal_chan_t c{&r}; bool asserted=false;
  try { stock_decode(&c); } catch (const std::runtime_error&) { asserted=true; }
  CHECK(asserted); // Reproduces the captured failure condition on stock source.
  for (uint32_t halted : {0u,USB_DWC_LL_INTR_CHAN_CHHLTD}) {
   r={mask|halted|USB_DWC_LL_INTR_CHAN_XFERCOMPL}; c={&r}; c.flags.halt_requested=true;
   CHECK(patched_decode(&c)==USB_DWC_HAL_CHAN_EVENT_ERROR);
   CHECK(!c.flags.active); CHECK(c.flags.halt_requested); CHECK(r.bits==0);
   auto expected = mask&1 ? USB_DWC_HAL_CHAN_ERROR_STALL : mask&2 ? USB_DWC_HAL_CHAN_ERROR_PKT_BBL :
                   mask&4 ? USB_DWC_HAL_CHAN_ERROR_BNA : USB_DWC_HAL_CHAN_ERROR_XCS_XACT;
   CHECK(c.error==expected);
   if (halted) { Registers sr{mask|halted|USB_DWC_LL_INTR_CHAN_XFERCOMPL}; usb_dwc_hal_chan_t sc{&sr};
    CHECK(stock_decode(&sc)==USB_DWC_HAL_CHAN_EVENT_ERROR); CHECK(sc.error==c.error); }
  }
 }
 for (bool halt_request : {false,true}) {
  Registers r{USB_DWC_LL_INTR_CHAN_CHHLTD}; usb_dwc_hal_chan_t c{&r};c.flags.halt_requested=halt_request;
  CHECK(patched_decode(&c)==(halt_request?USB_DWC_HAL_CHAN_EVENT_HALT_REQ:USB_DWC_HAL_CHAN_EVENT_CPLT));
  CHECK(!c.flags.active && !c.flags.halt_requested);
 }
 Registers r{USB_DWC_LL_INTR_CHAN_XFERCOMPL}; usb_dwc_hal_chan_t c{&r};
 CHECK(patched_decode(&c)==USB_DWC_HAL_CHAN_EVENT_NONE);CHECK(r.disabled);CHECK(c.flags.active);
 std::printf("PASS actual SDK decoder stock assertion reproduced; upstream correction checks=%u\n",checks);
}
"""
(out/"decoder.cpp").write_text(preamble+stock+"\n"+patched+tests)
(out/"CMakeLists.txt").write_text('cmake_minimum_required(VERSION 3.16)\nproject(usb_hal_error_order LANGUAGES CXX)\nadd_executable(usb_hal_error_order decoder.cpp)\ntarget_compile_features(usb_hal_error_order PRIVATE cxx_std_17)\n')
(out/"provenance.json").write_text(json.dumps(dict(sdk_source=str(source_path),source_sha256=hashlib.sha256(source_path.read_bytes()).hexdigest(),upstream_commit="63b468539f86f759ae1075ed277b2da65eccbdfe",change="remove only the early CHHLTD error assertion",limits="Host register/interrupt mocks; no physical timing or USB recovery proof"),indent=2))
print("Generated stock/patched actual-function harness; SDK unchanged")
