// -*- c-basic-offset: 2; related-file-name: "../../lib/variableenv.cc" -*-
#ifndef CLICK_VARIABLEENVIRONMENT_HH
#define CLICK_VARIABLEENVIRONMENT_HH
#include <click/string.hh>
#include <click/vector.hh>
CLICK_DECLS

class VariableEnvironment { public:
  
  VariableEnvironment()				{ }
  VariableEnvironment(const String &suffix);
  VariableEnvironment(const VariableEnvironment &, const String &suffix);

  operator bool() const			{ return _formals.size() != 0; }
  const String &prefix() const		{ return _prefix; }
  int depth() const			{ return _depths.size() ? _depths.back() : -1; }

  void enter(const VariableEnvironment &);
  void enter(const Vector<String> &formals, const Vector<String> &values, int depth);
  void limit_depth(int);
  
  String interpolate(const String &) const;

 private:

  String _prefix;
  Vector<String> _formals;
  Vector<String> _values;
  Vector<int> _depths;

};

CLICK_ENDDECLS
#endif
