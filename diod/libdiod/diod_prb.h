typedef struct diod_prb_struct *diod_prb_t;

diod_prb_t diod_prb_create (void);
int diod_prb_listen (List l, diod_prb_t prb);
void diod_prb_accept_one (Npsrv *srv, diod_prb_t prb);
void diod_prb_shutdown (diod_prb_t prb);
void diod_prb_destroy (diod_prb_t prb);

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
