// Reserva virtual 15–60 GiB, sin contenido en disco ni páginas físicas.
// El cargador la instala antes de que libSystem cree las zonas de malloc.
.zerofill __GUEST,__guest,_nx_guest_reservation,0xB40000000,14
