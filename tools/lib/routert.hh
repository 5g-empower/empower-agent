// -*- c-basic-offset: 4 -*-
#ifndef CLICK_ROUTERT_HH
#define CLICK_ROUTERT_HH
#include "elementt.hh"
#include "eclasst.hh"
#include <click/error.hh>
#include <click/hashmap.hh>
#include <click/archive.hh>
typedef HashMap<String, int> StringMap;

class RouterT { public:

    RouterT(ElementClassT * = 0, RouterT * = 0);
    virtual ~RouterT();

    void use()				{ _use_count++; }
    void unuse()			{ if (--_use_count <= 0) delete this; }

    ElementClassT *enclosing_type() const { return _enclosing_type; }

    void check() const;
    bool is_flat() const;

    ElementClassT *locally_declared_type(const String &) const;
    inline ElementClassT *declared_type(const String &) const;
    ElementClassT *install_type(ElementClassT *, bool install_name = false);
    void collect_primitive_classes(HashMap<String, int> &) const;
    void collect_active_types(Vector<ElementClassT *> &) const;

    int ntypes() const			{ return _etypes.size(); }
    ElementClassT *eclass(int i)	{ return _etypes[i].eclass; }
    
    class iterator;
    class const_iterator;
    class type_iterator;
    class const_type_iterator;
    inline iterator begin_elements();
    inline const_iterator begin_elements() const;
    inline type_iterator begin_elements(ElementClassT *);
    inline const_type_iterator begin_elements(ElementClassT *) const;
    inline iterator end_elements();
    inline const_iterator end_elements() const;
    
    int nelements() const		{ return _elements.size(); }
    int real_element_count() const	{ return _real_ecount; }
    
    inline const ElementT *element(const String &) const;
    inline const ElementT *elt(const String &) const;
    inline ElementT *element(const String &);
    inline ElementT *elt(const String &);
    int eindex(const String &s) const	{ return _element_name_map[s]; }
    
    const ElementT *element(int i) const{ return _elements[i]; }
    const ElementT *elt(int i) const	{ return _elements[i]; }
    ElementT *element(int i)		{ return _elements[i]; }
    ElementT *elt(int i)		{ return _elements[i]; }
    
    bool elive(int i) const		{ return _elements[i]->live(); }
    bool edead(int i) const		{ return _elements[i]->dead(); }
    inline String ename(int) const;
    inline ElementClassT *etype(int) const;
    inline String etype_name(int) const;

    ElementT *get_element(const String &name, ElementClassT *, const String &configuration, const String &landmark);
    ElementT *add_anon_element(ElementClassT *, const String &configuration = String(), const String &landmark = String());
    void change_ename(int, const String &);
    void deanonymize_elements();
    void free_element(ElementT *);
    void free_dead_elements();

    void set_new_eindex_collector(Vector<int> *v) { _new_eindex_collector=v; }

    int nconnections() const			{ return _conn.size(); }
    const Vector<ConnectionT> &connections() const { return _conn; }
    const ConnectionT &connection(int c) const	{ return _conn[c]; }
    bool connection_live(int c) const		{ return _conn[c].live(); }

    void add_tunnel(const String &, const String &, const String &, ErrorHandler *);

    bool add_connection(const PortT &, const PortT &, const String &landmark = String());
    inline bool add_connection(ElementT *, int, ElementT *, int, const String &landmark = String());
    void kill_connection(int);
    void kill_bad_connections();
    void compact_connections();

    void add_requirement(const String &);
    void remove_requirement(const String &);
    const Vector<String> &requirements() const	{ return _requirements; }

    void add_archive(const ArchiveElement &);
    int narchive() const			{ return _archive.size(); }
    int archive_index(const String &s) const	{ return _archive_map[s]; }
    const Vector<ArchiveElement> &archive() const{ return _archive; }
    ArchiveElement &archive(int i)		{ return _archive[i]; }
    const ArchiveElement &archive(int i) const	{ return _archive[i]; }
    inline ArchiveElement &archive(const String &s);
    inline const ArchiveElement &archive(const String &s) const;

    inline bool has_connection(const PortT &, const PortT &) const;
    int find_connection(const PortT &, const PortT &) const;
    void change_connection_to(int, PortT);
    void change_connection_from(int, PortT);
    bool find_connection_from(const PortT &, PortT &) const;
    void find_connections_from(const PortT &, Vector<PortT> &) const;
    void find_connections_from(const PortT &, Vector<int> &) const;
    void find_connections_to(const PortT &, Vector<PortT> &) const;
    void find_connections_to(const PortT &, Vector<int> &) const;
    void find_connection_vector_from(ElementT *, Vector<int> &) const;
    void find_connection_vector_to(ElementT *, Vector<int> &) const;

    bool insert_before(const PortT &, const PortT &);
    bool insert_after(const PortT &, const PortT &);
    inline bool insert_before(ElementT *, const PortT &);
    inline bool insert_after(ElementT *, const PortT &);

    void add_components_to(RouterT *, const String &prefix = String()) const;

    void remove_duplicate_connections();
    void remove_dead_elements(ErrorHandler * = 0);

    void remove_compound_elements(ErrorHandler *);
    void remove_tunnels(ErrorHandler * = 0);

    void expand_into(RouterT *, const VariableEnvironment &, ErrorHandler *);
    void flatten(ErrorHandler *);

    void unparse(StringAccum &, const String & = String()) const;
    void unparse_requirements(StringAccum &, const String & = String()) const;
    void unparse_declarations(StringAccum &, const String & = String()) const;
    void unparse_connections(StringAccum &, const String & = String()) const;
    String configuration_string() const;

  private:
  
    struct Pair {
	int from;
	int to;
	Pair() : from(-1), to(-1) { }
	Pair(int f, int t) : from(f), to(t) { }
    };

    struct ElementType {
	ElementClassT * const eclass;
	int scope_cookie;
	int prev_name;
	ElementType(ElementClassT *c, int sc, int pn) : eclass(c), scope_cookie(sc), prev_name(pn) { assert(eclass); eclass->use(); }
	ElementType(const ElementType &o) : eclass(o.eclass), scope_cookie(o.scope_cookie), prev_name(o.prev_name) { eclass->use(); }
	~ElementType()			{ eclass->unuse(); }
	const String &name() const	{ return eclass->name(); }
      private:
	ElementType &operator=(const ElementType &);
    };

    int _use_count;
    ElementClassT *_enclosing_type;

    RouterT *_enclosing_scope;
    int _enclosing_scope_cookie;
    int _scope_cookie;
    
    StringMap _etype_map;
    Vector<ElementType> _etypes; // Might not contain every element's type.

    StringMap _element_name_map;
    Vector<ElementT *> _elements;
    ElementT *_free_element;
    int _real_ecount;
    Vector<int> *_new_eindex_collector;

    Vector<ConnectionT> _conn;
    Vector<Pair> _first_conn;
    int _free_conn;

    Vector<String> _requirements;

    StringMap _archive_map;
    Vector<ArchiveElement> _archive;

    RouterT(const RouterT &);
    RouterT &operator=(const RouterT &);

    ElementClassT *declared_type(const String &, int scope_cookie) const;
    void update_noutputs(int);
    void update_ninputs(int);
    ElementT *add_element(const ElementT &);
    void assign_element_name(int);
    void free_connection(int ci);
    void unlink_connection_from(int ci);
    void unlink_connection_to(int ci);
    void expand_tunnel(Vector<PortT> *port_expansions, const Vector<PortT> &ports, bool is_output, int which, ErrorHandler *) const;
    String interpolate_arguments(const String &, const Vector<String> &) const;

};

class RouterT::const_iterator { public:
    operator bool() const		{ return _e; }
    int idx() const			{ return _e->idx(); }
    void operator++(int)		{ if (_e) step(_e->router(), idx()+1);}
    void operator++()			{ (*this)++; }
    operator const ElementT *() const	{ return _e; }
    const ElementT *operator->() const	{ return _e; }
    const ElementT &operator*() const	{ return *_e; }
  private:
    const ElementT *_e;
    const_iterator()			: _e(0) { }
    const_iterator(const RouterT *r, int ei) { step(r, ei); }
    void step(const RouterT *, int);
    friend class RouterT;
    friend class RouterT::iterator;
};

class RouterT::iterator : public RouterT::const_iterator { public:
    operator ElementT *() const		{ return const_cast<ElementT *>(_e); }
    ElementT *operator->() const	{ return const_cast<ElementT *>(_e); }
    ElementT &operator*() const		{ return const_cast<ElementT &>(*_e); }
  private:
    iterator()				: const_iterator() { }
    iterator(RouterT *r, int ei)	: const_iterator(r, ei) { }
    friend class RouterT;
};

class RouterT::const_type_iterator { public:
    operator bool() const		{ return _e; }
    int idx() const			{ return _e->idx(); }
    inline void operator++(int);
    inline void operator++();
    operator const ElementT *() const	{ return _e; }
    const ElementT *operator->() const	{ return _e; }
    const ElementT &operator*() const	{ return *_e; }
  private:
    const ElementT *_e;
    const_type_iterator()		: _e(0) { }
    const_type_iterator(const RouterT *r, ElementClassT *t, int i) { step(r, t, i); }
    void step(const RouterT *, ElementClassT *, int);
    friend class RouterT;
    friend class RouterT::type_iterator;
};

class RouterT::type_iterator : public RouterT::const_type_iterator { public:
    operator ElementT *() const		{ return const_cast<ElementT *>(_e); }
    ElementT *operator->() const	{ return const_cast<ElementT *>(_e); }
    ElementT &operator*() const		{ return const_cast<ElementT &>(*_e); }
  private:
    type_iterator()			: const_type_iterator() { }
    type_iterator(RouterT *r, ElementClassT *t, int ei)	: const_type_iterator(r, t, ei) { }
    friend class RouterT;
};


inline RouterT::iterator
RouterT::begin_elements()
{
    return iterator(this, 0);
}

inline RouterT::const_iterator
RouterT::begin_elements() const
{
    return const_iterator(this, 0);
}

inline RouterT::type_iterator
RouterT::begin_elements(ElementClassT *t)
{
    return type_iterator(this, t, 0);
}

inline RouterT::const_type_iterator
RouterT::begin_elements(ElementClassT *t) const
{
    return const_type_iterator(this, t, 0);
}

inline RouterT::const_iterator
RouterT::end_elements() const
{
    return const_iterator();
}

inline RouterT::iterator
RouterT::end_elements()
{
    return iterator();
}

inline void
RouterT::const_type_iterator::operator++(int)
{
    if (_e)
	step(_e->router(), _e->type(), _e->idx() + 1);
}

inline void
RouterT::const_type_iterator::operator++()
{
    (*this)++;
}

inline const ElementT *
RouterT::element(const String &s) const
{
    int i = _element_name_map[s];
    return (i >= 0 ? _elements[i] : 0);
}

inline const ElementT *
RouterT::elt(const String &s) const
{
    return element(s);
}

inline ElementT *
RouterT::element(const String &s)
{
    int i = _element_name_map[s];
    return (i >= 0 ? _elements[i] : 0);
}

inline ElementT *
RouterT::elt(const String &s)
{
    return element(s);
}

inline String
RouterT::ename(int e) const
{
    return _elements[e]->name();
}

inline ElementClassT *
RouterT::etype(int e) const
{
    return _elements[e]->type();
}

inline String
RouterT::etype_name(int e) const
{
    return _elements[e]->type()->name();
}

inline ElementClassT *
RouterT::declared_type(const String &name) const
{
    return declared_type(name, 0x7FFFFFFF);
}

inline bool
RouterT::add_connection(ElementT *from_elt, int from_port, ElementT *to_elt,
			int to_port, const String &landmark)
{
    return add_connection(PortT(from_elt, from_port), PortT(to_elt, to_port), landmark);
}

inline bool
RouterT::has_connection(const PortT &hfrom, const PortT &hto) const
{
    return find_connection(hfrom, hto) >= 0;
}

inline bool
RouterT::insert_before(ElementT *e, const PortT &h)
{
    return insert_before(PortT(e, 0), h);
}

inline bool
RouterT::insert_after(ElementT *e, const PortT &h)
{
    return insert_after(PortT(e, 0), h);
}

inline ArchiveElement &
RouterT::archive(const String &name)
{
    return _archive[_archive_map[name]];
}

inline const ArchiveElement &
RouterT::archive(const String &name) const
{
    return _archive[_archive_map[name]];
}

inline ElementClassT *
ElementT::enclosing_type() const
{
    return (_owner ? _owner->enclosing_type() : 0);
}

inline bool
operator==(const RouterT::const_iterator &i, const RouterT::const_iterator &j)
{
    return i.operator->() == j.operator->();
}

inline bool
operator!=(const RouterT::const_iterator &i, const RouterT::const_iterator &j)
{
    return i.operator->() != j.operator->();
}

inline bool
operator==(const RouterT::type_iterator &i, const RouterT::type_iterator &j)
{
    return i.operator->() == j.operator->();
}

inline bool
operator!=(const RouterT::type_iterator &i, const RouterT::type_iterator &j)
{
    return i.operator->() != j.operator->();
}

inline bool
operator!=(const RouterT::type_iterator &i, const RouterT::const_iterator &j)
{
    return i.operator->() != j.operator->();
}

#endif
