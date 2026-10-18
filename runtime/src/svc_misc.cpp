// Small services that only need to answer "yes, fine": parental controls, add-on content, logging, power.
#include <cstdlib>
#include <mutex>
#include "ipc.h"
#include "lm_packet.h"

namespace nx {

namespace {

std::shared_ptr<Service> make(const char* name, std::function<void(Service&)> setup = {}) {
    auto s = std::make_shared<Service>(name);
    if (setup) setup(*s);
    return s;
}

std::shared_ptr<Service> parental_control() {
    return make("IParentalControlService", [](Service& s) {
        s.cmds[1403] = [](IpcCtx& c) { c.push<uint8_t>(0); return 0u; };  // IsRestrictionEnabled
        s.cmds[1001] = [](IpcCtx&) { return 0u; };                          // CheckFreeCommunicationPermission
    });
}

void pctl(Service& s) {
    s.cmds[0] = [](IpcCtx& c) { c.push_object(parental_control()); return 0u; };  // CreateService
    s.cmds[1] = [](IpcCtx& c) { c.push_object(parental_control()); return 0u; };  // CreateServiceWithoutInitialize
}

ServiceRegistrar r_pctl("pctl", [] { return make("pctl", pctl); });
ServiceRegistrar r_pctl_s("pctl:s", [] { return make("pctl:s", pctl); });
ServiceRegistrar r_pctl_a("pctl:a", [] { return make("pctl:a", pctl); });
ServiceRegistrar r_pctl_r("pctl:r", [] { return make("pctl:r", pctl); });

struct LmState {
    std::mutex lock;
    std::map<uint64_t, std::string> messages;
};

std::shared_ptr<Service> logger() {
    auto state = std::make_shared<LmState>();
    return make("ILogger", [state](Service& service) {
        service.cmds[0] = [state](IpcCtx& context) {
            const Buffer buffer = context.in_buf(0);
            if (!buffer.addr || buffer.size > 0x100000) return 0u;
            LmFragment fragment;
            if (!parse_lm_packet({reinterpret_cast<const uint8_t*>(buffer.addr), size_t(buffer.size)}, fragment)) {
                log("LM: paquete incompleto o incompatible");
                return 0u;
            }
            std::lock_guard guard(state->lock);
            auto& message = state->messages[fragment.thread];
            if (fragment.flags & 1) message.clear();
            if (message.size() + fragment.message.size() <= 0x100000) message += fragment.message;
            else message.clear();
            if (fragment.flags & 2) {
                while (!message.empty() && (message.back() == '\0' || message.back() == '\n' || message.back() == '\r'))
                    message.pop_back();
                if (!message.empty()) log("[invitado:%u] %s", fragment.severity, message.c_str());
                state->messages.erase(fragment.thread);
            }
            return 0u;
        };
        service.cmds[1] = [](IpcCtx&) { return 0u; };  // El destino del host es su registro local.
    });
}

ServiceRegistrar r_lm("lm", [] {
    return make("lm", [](Service& s) {
        s.cmds[0] = [](IpcCtx& c) { c.push_object(logger()); return 0u; };  // OpenLogger
    });
});

ServiceRegistrar r_apm("apm", [] {
    return make("apm", [](Service& s) {
        s.cmds[0] = [](IpcCtx& c) { c.push_object(make("ISession")); return 0u; };  // OpenSession
        s.cmds[1] = [](IpcCtx& c) {  // GetPerformanceMode: boost (docked), normal with NX_HANDHELD (svc_am.cpp)
            c.push<uint32_t>(std::getenv("NX_HANDHELD") ? 0 : 1);
            return 0u;
        };
    });
});

ServiceRegistrar r_aoc("aoc:u", [] {
    return make("aoc:u", [](Service& s) {
        s.cmds[2] = [](IpcCtx& c) { c.push<uint32_t>(0); return 0u; };  // CountAddOnContent (old)
        s.cmds[3] = [](IpcCtx& c) { c.push<uint32_t>(0); return 0u; };  // ListAddOnContent (old)
        s.cmds[7] = [](IpcCtx& c) { c.push<uint32_t>(0); return 0u; };
        s.cmds[8] = [](IpcCtx& c) {                                       // GetAddOnContentListChangedEvent
            auto e = std::make_shared<KEvent>();
            e->state = std::make_shared<EventState>();
            c.out_copy.push_back(handle_add(e));
            return 0u;
        };
    });
});

}  // namespace

}  // namespace nx
