#ifndef MOUSE_H
#define MOUSE_H

int vmouse_init(void);
void vmouse_exit(void);

void rkkdr_send_mouse_btn_event(unsigned int code, int value);

#endif /* MOUSE_H */
