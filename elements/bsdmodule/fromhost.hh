// -*- c-basic-offset: 4 -*-
#ifndef CLICK_FROMHOST_HH
#define CLICK_FROMHOST_HH
#include "elements/bsdmodule/anydevice.hh"

class FromHost : public AnyDevice {

  public:
    FromHost();
    ~FromHost();
    const char *class_name() const      { return "FromHost"; }
    const char *processing() const      { return PUSH; }

    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);
    bool run_task();
    struct ifqueue *_inq;
  
  private:
    static const int QSIZE = 511;
    unsigned _burst;
};

#endif
