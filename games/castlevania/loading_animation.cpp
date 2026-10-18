#if defined(__APPLE__)
#include <nx/runtime.h>
#include <atomic>

void nx_castlevania_loading_material(nx::CPU& c, void (*original)(nx::CPU&)) {
    const uint64_t self=c.x[0];
    const uint64_t settings=self ? nx::ld<uint64_t>(self+0x38) : 0;
    const uint64_t material=settings ? nx::ld<uint64_t>(settings+0x18) : 0;
    if(material) {
        nx::CPU frame=c;frame.x[0]=self;frame.x[1]=0;
        nx::call(frame,NX_BASE+0x1dcc470); // GetCurrentAnimationFrameIndex respeta la animación invertida.
        nx::CPU prepare=c;prepare.x[0]=material;
        prepare.x[1]=nx::ld<uint32_t>(self+0x48);prepare.x[2]=0;
        prepare.v[0].f32[0]=frame.v[0].f32[0];
        // El material debe tener el primer índice antes de que Image.set_material lo haga visible.
        nx::call(prepare,NX_BASE+0x4e1aba0);
        static std::atomic<unsigned> logged{0};
        if(logged.fetch_add(1)<8)nx::log("loading: material inicializado antes de asignar, índice %g",frame.v[0].f32[0]);
    }
    original(c);
}
#endif
