// -*- c-basic-offset: 4 -*-
#include <click/config.h>
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/error.hh>
#include "trieiplookup.hh"
CLICK_DECLS

static int
prefix_order_compar(const void *athunk, const void *bthunk)
{
    const TrieIPLookup::Prefix* a = (const TrieIPLookup::Prefix*) athunk;
    const TrieIPLookup::Prefix* b = (const TrieIPLookup::Prefix*) bthunk;

    if (a->addr.addr() == b->addr.addr())
        return a->mask.mask_to_prefix_len() - b->mask.mask_to_prefix_len();
    else if (ntohl(a->addr.addr()) > ntohl(b->addr.addr()))
        return 1;
    else
        return -1;
}

TrieIPLookup::TrieIPLookup()
{
    MOD_INC_USE_COUNT;
    add_input();

    // initialize _default_rope
    _default_rope.push_back(0);
    _default_rope.push_back(1);
    _default_rope.push_back(2);
    _default_rope.push_back(4);
    _default_rope.push_back(8);
    _default_rope.push_back(16);

    // default route
    _route_vector.push_back(Prefix(0, 0, 0, -1));
}

TrieIPLookup::~TrieIPLookup()
{
    MOD_DEC_USE_COUNT;
}

void TrieIPLookup::notify_noutputs(int n)
{
    set_noutputs(n);
}

int
TrieIPLookup::initialize(ErrorHandler *)
{
    return 0;
}

int
TrieIPLookup::configure(Vector<String> &conf, ErrorHandler *errh)
{
    int before = errh->nerrors();
    for (int i = 0; i < conf.size(); i++) {
        Vector<String> words;
        cp_spacevec(conf[i], words);

        IPAddress dst, mask, gw;
        int32_t port;
        bool ok = false;
        if ((words.size() == 2 || words.size() == 3)
            && cp_ip_prefix(words[0], &dst, &mask, true, this)
            && cp_integer(words.back(), &port)) {
            if (words.size() == 3)
                ok = cp_ip_address(words[1], &gw, this);
            else
                ok = true;
        }

        if (ok && port >= 0) {
            if (port >= noutputs())
                errh->error("port number out of range");

            dst &= mask;
            _route_vector.push_back(Prefix(dst, mask, gw, port));
        } else
            errh->error("argument %d should be `DADDR/MASK [GATEWAY] OUTPUT'", i+1);
    }

    if (errh->nerrors() == before) {
        configure_route_vector();
        build_main();
        return 0;
    } else
        return -1;
}

inline void
TrieIPLookup::configure_route_vector()
{
    // sort _route_vector
    click_qsort(&_route_vector[0], _route_vector.size(), sizeof(Prefix), prefix_order_compar);

    // get rid of duplicates in _route_vector
    int n_last = 0;
    int n_current = 1;
    while (n_current < _route_vector.size()) {
        if (_route_vector[n_last].addr != _route_vector[n_current].addr ||
            _route_vector[n_last].mask != _route_vector[n_current].mask) {
            n_last++;
            if (n_last < n_current)
                _route_vector[n_last] = _route_vector[n_current];
        }
        n_current++;
    }

    for (int i = 0; i < _route_vector.size() - n_last - 1; i++) {
        _route_vector.pop_back();
    }
}

inline void
TrieIPLookup::build_init()
{
    // make sure there's a root node
    assert (_route_vector[0].mask.mask_to_prefix_len() == 0);

    // fill _trie_vector
    for (int i = 0; i < _route_vector.size(); i++) {
        const Prefix prefix = _route_vector[i];
        _trie_vector.push_back(TrieNode(prefix.addr, prefix.mask, prefix.gw,
                                        prefix.output, true, i));
    }
}

inline int
TrieIPLookup::build_exists_middle(const TrieNode& parent, const TrieNode& child)
{
    // no need for middle child
    if (-1 == parent.left_child) {
        assert(-1 == parent.right_child);
        return 0;
    }

    // sibling to with common prefix with
    const TrieNode &sibling = -1 != parent.right_child ?
        _trie_vector[parent.right_child] : _trie_vector[parent.left_child];

    // just making sure
    assert(parent.mask.mask_to_prefix_len() < child.mask.mask_to_prefix_len());
    assert(parent.mask.mask_to_prefix_len() < sibling.mask.mask_to_prefix_len());
    assert((child.addr & parent.mask) == parent.addr);
    assert((sibling.addr & parent.mask) == parent.addr);

    int lower_bound = parent.mask.mask_to_prefix_len() + 1;
    int upper_bound = child.mask.mask_to_prefix_len();

    for (int i = lower_bound; i <= upper_bound; i++) {
        IPAddress newmask = IPAddress::make_prefix(i);
        if ((newmask & child.addr) != (newmask & sibling.addr)) {
            if (i == lower_bound) {
                assert(-1 == parent.right_child);
                return 0;
            } else {
                return i - 1;
            }
        }
    }
    assert(false);
    return 0;
}

// returns whether there exists a middle
inline void
TrieIPLookup::build_middle(int prefix_length, const TrieNode& parent, const TrieNode& child)
{
    assert(-1 != parent.left_child);
    const TrieNode &sibling = -1 != parent.right_child ?
        _trie_vector[parent.right_child] : _trie_vector[parent.left_child];

    // create middle node
    IPAddress mask = IPAddress::make_prefix(prefix_length);
    TrieNode middle(child.addr & mask, mask, parent.gw, parent.output, false, _trie_vector.size());
    middle.left_child = sibling.index;
    middle.right_child = child.index;
    middle.parent = parent.index;
    _trie_vector.push_back(middle);

    // make all child/parent connections
    if (-1 != parent.right_child) {
        _trie_vector[parent.index].right_child = middle.index;
    } else {
        _trie_vector[parent.index].left_child = middle.index;
    }

    _trie_vector[sibling.index].parent = middle.index;
    _trie_vector[child.index].parent = middle.index;

    // just making sure
    check_trie_node(middle);
    check_trie_node(_trie_vector[middle.left_child]);
    check_trie_node(_trie_vector[middle.right_child]);
}


inline void
TrieIPLookup::build_trie()
{
    if (_trie_vector.size() == 0)
        return;

    TrieNode cn = _trie_vector[0];

    // take care of the rest
    int _trie_vector_size = _trie_vector.size();
    for (int i = 1; i < _trie_vector_size; i++) {

        check_trie(_trie_vector[0]);

        TrieNode tn = _trie_vector[i];

        while ((tn.mask.mask_to_prefix_len() <= cn.mask.mask_to_prefix_len()) ||
               ((tn.addr & cn.mask) != cn.addr)) {
            // while the above condition is true, tn should not be a child of cn
            cn = _trie_vector[cn.parent];
        }

        // make sure tn should be a child of cn
        assert((tn.addr & cn.mask) == cn.addr);

        // should we create a fake node?
        int prefix_length = build_exists_middle(cn, tn);
        if (prefix_length) {
            check_trie_node(_trie_vector[cn.left_child]);
            build_middle(prefix_length, cn, tn);
        } else {
            assert(-1 == cn.right_child);
            if (-1 != cn.left_child) {
                _trie_vector[cn.index].right_child = tn.index;
            } else {
                _trie_vector[cn.index].left_child = tn.index;
            }
            _trie_vector[tn.index].parent = cn.index;
            check_trie_node(_trie_vector[tn.index]);
        }
        cn = _trie_vector[tn.index];
    }
}

void
TrieIPLookup::build_main()
{
    Timestamp ts = Timestamp::now();
    click_chatter("starting to init data structure: %d\n", ts.sec());

    check_route_vector_sorted();

    // build initial data structure
    _trie_vector.clear();
    build_init();
    check_init();

    ts = Timestamp::now();
    click_chatter("starting to build trie: %d\n", ts.sec());

    // build trie
    build_trie();
    check_trie(_trie_vector[0]);

    ts = Timestamp::now();
    click_chatter("starting to set children lengths: %d\n", ts.sec());

    // set children lengths
    build_children_lengths(0);
    check_lengths(_trie_vector[0]);

    ts = Timestamp::now();
    click_chatter("starting to rebuild hash: %d\n", ts.sec());

    // rebuild hash
    for (int i = 0; i <= 32; i++) {
        _lengths_array[i].clear();
    }
    build_hash(0);

    ts = Timestamp::now();
    click_chatter("done initializaton: %d\n", ts.sec());

    _trie_vector.clear();
}

void
TrieIPLookup::build_hash(int index)
{
    if (-1 == index) return;

    TrieNode &tn = _trie_vector[index];

    // maybe add this node to the hashmap
    build_hash_node(tn, _default_rope, 32);

    // call this method recursively on all the children
    build_hash(tn.left_child);
    build_hash(tn.right_child);
}

void
TrieIPLookup::build_hash_node(TrieNode &tn, const Rope& rope, int upper_bound_inclusive)
{
    int index;
    int length;
    LengthHash* lh;
    Marker* p_marker;

    if (rope.size() == 0 && !tn.is_real)
        return;
    
    // get the first index from the rope
    assert(rope.size() > 0);
    index = rope.back();

    // get length of the bitmask
    length = tn.mask.mask_to_prefix_len();

    if (length < index) {
        // our final place is somewhere above here, so follow the rope
        Rope new_rope(rope);
        new_rope.pop_back();
        build_hash_node(tn, new_rope, index-1);
    } else {
        IPAddress current_mask = IPAddress::make_prefix(index);

        // get the hashmap corresponding to that index
        lh = &_lengths_array[index];

        // look for a marker/prefix
        p_marker = lh->findp(tn.addr & current_mask);

        if (NULL != p_marker) {
            // assert this isn't our final resting place
            assert(index != length);

            // go to the new trie and continue searching
            build_hash_node(tn, p_marker->rope, upper_bound_inclusive);
        } else {
            // we are the first to visit this level, so create marker and insert it
            Marker new_marker;

            build_hash_marker(new_marker, tn, length, index, upper_bound_inclusive);
            lh->insert(tn.addr & current_mask, new_marker);

            // continue
            if (length > index) {
                build_hash_node(tn, new_marker.rope, upper_bound_inclusive);
            }
        }
    }
}

inline void
TrieIPLookup::build_hash_marker(Marker &new_marker, TrieNode tn, int n_prefix_length,
                                int n_array_index, int upper_bound_inclusive)
{
    // set the rope for our new marker
    Vector<int> lengths_vec;
    if (n_prefix_length != n_array_index) lengths_vec.push_back(n_prefix_length);
    int i;
    for (i = 0; i < 32; i++) {
        if (tn.children_lengths & (1 << i)) {
            if (n_prefix_length + i + 1 <= upper_bound_inclusive)
                lengths_vec.push_back(n_prefix_length + i + 1);
        }
    }
    
    int positions[6];
    int end = 0;
    
    i = lengths_vec.size();
    while (i > 0) {
        i /= 2;
        positions[end] = lengths_vec[i];
        end++;
    }
    
    for (i = 0; i < end; i++) {
        new_marker.rope.push_back(positions[end - i - 1]);
    }

    // set gw & output
    while(n_array_index < tn.mask.mask_to_prefix_len()) {
        tn = _trie_vector[tn.parent];
    }
    new_marker.gw = tn.gw;
    new_marker.output = tn.output;
}

void
TrieIPLookup::build_children_lengths(int index)
{
    // initialize it to 0
    TrieNode &tn = _trie_vector[index];
    tn.children_lengths = 0;

    // loop through children and get their lengths
    if (-1 != tn.left_child) {
        build_children_lengths(tn.left_child);
        TrieNode &left_child = _trie_vector[tn.left_child];
	
        // update my variable
        int distance = left_child.mask.mask_to_prefix_len() - tn.mask.mask_to_prefix_len();
        tn.children_lengths |= (left_child.children_lengths << distance);

        // if this child is real, then add it also
        if (left_child.is_real) {
            tn.children_lengths |= (1 << (distance-1));
        }
    }

    if (-1 != tn.right_child) {
        build_children_lengths(tn.right_child);
        TrieNode &right_child = _trie_vector[tn.right_child];
	
        // update my variable
        int distance = right_child.mask.mask_to_prefix_len() - tn.mask.mask_to_prefix_len();
        tn.children_lengths |= (right_child.children_lengths << distance);

        // if this child is real, then add it also
        if (right_child.is_real) {
            tn.children_lengths |= (1 << (distance-1));
        }
    }
}

void
TrieIPLookup::add_handlers()
{
    add_write_handler("add", add_route_handler, 0);
    add_write_handler("remove", remove_route_handler, 0);
    add_write_handler("ctrl", ctrl_handler, 0);
    add_read_handler("table", table_handler, 0);
}

// returns the position in the vector pf belongs
inline int
TrieIPLookup::binary_search(const Vector<Prefix> &vec, const Prefix &pf)
{
    long n_upperbound = vec.size() - 1;
    long n_lowerbound = 0;
    long n_middle;

    while (n_upperbound >= n_lowerbound) {
        n_middle = (n_upperbound + n_lowerbound) / 2;
        int n_compare = prefix_order_compar(&pf, &_route_vector[n_middle]);
        if (0 == n_compare) {
            n_lowerbound = n_middle;
            break;
        } else if (0 < n_compare)
            n_lowerbound = n_middle + 1;
        else
            n_upperbound = n_middle - 1;
    }
    return n_lowerbound;
}

int
TrieIPLookup::add_route(IPAddress addr, IPAddress mask, IPAddress gw,
                        int output, ErrorHandler *errh)
{
    if (output < 0 || output >= noutputs()) {
        errh->error("port number out of range");
        return -1;
    }
    addr &= mask;
    Prefix pf(addr, mask, gw, output);
    
    // add route to sorted _route_vector
    long n_position = binary_search(_route_vector, pf);

    if (_route_vector.size() == n_position) {
        _route_vector.push_back(pf);
    } else {
        int n_compare = prefix_order_compar(&pf, &_route_vector[n_position]);
        if (0 == n_compare)
            _route_vector[n_position] = pf;
        else {
            assert(n_compare < 0);
            _route_vector.push_back(_route_vector[_route_vector.size() - 1]);
            for (int i = _route_vector.size() - 3; i >= n_position; i--) {
                _route_vector[i + 1] = _route_vector[i];
            }
            _route_vector[n_position] = pf;
        }
    }

    // build the rest of the data structure
    build_main();
    return 0;
}

int
TrieIPLookup::remove_route(IPAddress addr, IPAddress mask, IPAddress gw,
                           int port, ErrorHandler *errh)
{
    addr &= mask;
    Prefix pf(addr, mask, gw, port);

    long n_position = binary_search(_route_vector, pf);
    if (_route_vector.size() == n_position ||
        _route_vector[n_position].addr != addr ||
         _route_vector[n_position].mask.mask_to_prefix_len() !=
	  mask.mask_to_prefix_len()) {
        errh->warning("no routes removed");
        return 1;
    }

    // shift elements forward
    for (int i = n_position; i < _route_vector.size() - 1; i++) {
        _route_vector[i] = _route_vector[i+1];
    }
    _route_vector.pop_back();

    // rebuild the rest of the data structure
    build_main();
    return 0;
}

int
TrieIPLookup::lookup_route(IPAddress a, IPAddress &gw) const
{
    int output = -1;
    Rope rope = _default_rope;
    while (rope.size() > 0) {
        int index = rope.back();
        IPAddress current_mask = IPAddress::make_prefix(index);
        Marker* pmarker = _lengths_array[index].findp(a & current_mask);

        if (0 == pmarker) {
            // we didn't find a marker at this level
            rope.pop_back();
        } else {
            // we found a marker, so update everything
            output = pmarker->output;
            gw = pmarker->gw;
            rope = pmarker->rope;
        }
    }
    return output;
}

String
TrieIPLookup::dump_routes() const
{
    StringAccum sa;
    if (_route_vector.size())
	sa << "# Active routes\n";
    for (int i = 0; i < _route_vector.size(); i++)
	    sa << _route_vector[i].unparse() << '\n';
    return sa.take_string();
}

void
TrieIPLookup::print_route_vector() const
{
    click_chatter("\n");
    for (int i = 0; i < _route_vector.size(); i++) {
        click_chatter("%s\n", _route_vector[i].unparse().cc());
    }
    click_chatter("\n");
}

void
TrieIPLookup::print_lengthhash(const LengthHash& lengthhash) const
{
    for (LengthHash::const_iterator it = lengthhash.begin(); it; it++)
        click_chatter("%s : %s", it.key().unparse().cc(), it.value().unparse().cc());
}

void
TrieIPLookup::print_trie(const TrieNode& tn) const
{
    click_chatter("%s", tn.unparse().cc());
}

void
TrieIPLookup::check_route_vector_sorted()
{
    if (_route_vector.size() == 0) return;

    // sorted and no duplicates
    Prefix last_prefix = _route_vector[0];

    for (int i = 1; i < _route_vector.size(); i++) {
        assert(prefix_order_compar(&last_prefix, &_route_vector[i]) < 0);
    }
}

void
TrieIPLookup::check_init()
{
    assert(_route_vector.size() == _trie_vector.size());
    if (!_trie_vector.size()) return;

    // elements should be in order and there should be no duplicates
    TrieNode last_node = _trie_vector[0];

    for (int i = 1; i < _trie_vector.size(); i++) {
        assert(last_node < _trie_vector[i]);
        last_node = _trie_vector[i];
    }

    // these should be the same elements as in _route_vector
    for (int i = 0; i < _trie_vector.size(); i++) {
        TrieNode tn = _trie_vector[i];
        bool ok = false;
        for (int i = 0; i < _route_vector.size(); i++) {
            Prefix prefix = _route_vector[i];
            if ((prefix.addr == tn.addr) && (prefix.mask == tn.mask) &&
                (prefix.gw == tn.gw) && (prefix.output == tn.output)) {
                ok = true;
                break;
            }
        }
        assert(ok);
    }

    for (int i = 0; i < _trie_vector.size(); i++) {
        TrieNode tn = _trie_vector[i];
        assert((tn.mask & tn.addr) == tn.addr);
    }
}

void
TrieIPLookup::check_trie_node(const TrieNode& node)
{
    assert(node.mask == (IPAddress::make_prefix(node.mask.mask_to_prefix_len())));
    assert ((node.mask & node.addr) == node.addr);

    if (node.mask.mask_to_prefix_len() == 0) {
        assert(-1 == node.parent);
        assert(0 == node.index);
    } else {
        assert(-1 != node.parent);
        assert(0 < node.index);
        TrieNode &parent = _trie_vector[node.parent];
        assert(parent.mask.mask_to_prefix_len() < node.mask.mask_to_prefix_len());
        assert((node.addr & parent.mask) == parent.addr);
    }
    if (node.mask.mask_to_prefix_len() && node.is_real)
        assert(node.output != -1);
}

void
TrieIPLookup::check_trie(const TrieNode& root)
{
    check_trie_node(root);

    if (-1 != root.left_child) {
        check_trie(_trie_vector[root.left_child]);
    }

    if (-1 != root.right_child) {
        check_trie(_trie_vector[root.right_child]);
    }
}

void
TrieIPLookup::check_lengths(const TrieNode& root) {
    uint32_t children_lengths = 0;
    if (-1 != root.left_child) {
        TrieNode &left_child = _trie_vector[root.left_child];
        int distance = left_child.mask.mask_to_prefix_len() - root.mask.mask_to_prefix_len();
        children_lengths |= (left_child.children_lengths << distance);

        if (left_child.is_real) {
            children_lengths |= (1 << (distance - 1));
        }

        check_lengths(left_child);
    }

    if (-1 != root.right_child) {
        TrieNode &right_child = _trie_vector[root.right_child];
        int distance = right_child.mask.mask_to_prefix_len() - root.mask.mask_to_prefix_len();
        children_lengths |= (right_child.children_lengths << distance);

        if (right_child.is_real) {
            children_lengths |= (1 << (distance - 1));
        }

        check_lengths(right_child);
    }

    assert (children_lengths == root.children_lengths);
}


#include <click/hashmap.cc>
#include <click/vector.cc>
CLICK_ENDDECLS
ELEMENT_REQUIRES(IPRouteTable)
EXPORT_ELEMENT(TrieIPLookup)
