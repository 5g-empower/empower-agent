/*
 * click-fastclassifier.cc -- specialize Click classifiers
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000-2001 Mazu Networks, Inc.
 * Copyright (c) 2001 International Computer Science Institute
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include <click/pathvars.h>

#include "routert.hh"
#include "lexert.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/clp.h>
#include <click/driver.hh>
#include "toolutils.hh"
#include "elementmap.hh"
#include "click-fastclassifier.hh"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cerrno>
#include <ctime>
#include <unistd.h>
#include <sys/stat.h>
#include <cstdarg>

#define HELP_OPT		300
#define VERSION_OPT		301
#define CLICKPATH_OPT		302
#define ROUTER_OPT		303
#define EXPRESSION_OPT		304
#define OUTPUT_OPT		305
#define KERNEL_OPT		306
#define USERLEVEL_OPT		307
#define SOURCE_OPT		308
#define CONFIG_OPT		309
#define REVERSE_OPT		310
#define COMBINE_OPT		311
#define COMPILE_OPT		312
#define QUIET_OPT		313
#define VERBOSE_OPT		314

static Clp_Option options[] = {
  { "classes", 0, COMPILE_OPT, 0, Clp_Negate },
  { "clickpath", 'C', CLICKPATH_OPT, Clp_ArgString, 0 },
  { "combine", 0, COMBINE_OPT, 0, Clp_Negate },
  { "config", 'c', CONFIG_OPT, 0, Clp_Negate },
  { "expression", 'e', EXPRESSION_OPT, Clp_ArgString, 0 },
  { "file", 'f', ROUTER_OPT, Clp_ArgString, 0 },
  { "help", 0, HELP_OPT, 0, 0 },
  { "kernel", 'k', KERNEL_OPT, 0, Clp_Negate },
  { "output", 'o', OUTPUT_OPT, Clp_ArgString, 0 },
  { "quiet", 'q', QUIET_OPT, 0, Clp_Negate },
  { "reverse", 'r', REVERSE_OPT, 0, Clp_Negate },
  { "source", 's', SOURCE_OPT, 0, Clp_Negate },
  { "user", 'u', USERLEVEL_OPT, 0, Clp_Negate },
  { "verbose", 'V', VERBOSE_OPT, 0, Clp_Negate },
  { "version", 'v', VERSION_OPT, 0, 0 },
};

static const char *program_name;
static String::Initializer string_initializer;
static String runclick_prog;
static String click_compile_prog;
static bool verbose;

void
short_usage()
{
  fprintf(stderr, "Usage: %s [OPTION]... [ROUTERFILE]\n\
Try `%s --help' for more information.\n",
	  program_name, program_name);
}

void
usage()
{
  printf("\
`Click-fastclassifier' transforms a router configuration by replacing generic\n\
Classifier elements with specific generated code. The resulting configuration\n\
has both Click-language files and object files.\n\
\n\
Usage: %s [OPTION]... [ROUTERFILE]\n\
\n\
Options:\n\
  -f, --file FILE               Read router configuration from FILE.\n\
  -e, --expression EXPR         Use EXPR as router configuration.\n\
  -o, --output FILE             Write output to FILE.\n\
      --no-combine              Do not combine adjacent Classifiers.\n\
      --no-classes              Do not generate FastClassifier elements.\n\
  -k, --kernel                  Compile into Linux kernel binary package.\n\
  -u, --user                    Compile into user-level binary package.\n\
  -s, --source                  Write source code only.\n\
  -c, --config                  Write new configuration only.\n\
  -r, --reverse                 Reverse transformation.\n\
  -q, --quiet                   Compile any packages quietly.\n\
  -C, --clickpath PATH          Use PATH for CLICKPATH.\n\
      --help                    Print this message and exit.\n\
  -v, --version                 Print version number and exit.\n\
\n\
Report bugs to <click@pdos.lcs.mit.edu>.\n", program_name);
}


// Classifier related stuff

static bool
combine_classifiers(RouterT *router, ElementT *from, int from_port, ElementT *to)
{
  ElementClassT *classifier_t = router->get_type("Classifier");
  assert(from->type() == classifier_t && to->type() == classifier_t);
  
  // find where `to' is heading for
  Vector<int> first_hop, second_hop;
  router->find_connection_vector_from(from, first_hop);
  router->find_connection_vector_from(to, second_hop);

  // check for weird configurations
  for (int i = 0; i < first_hop.size(); i++)
    if (first_hop[i] < 0)
      return false;
  for (int i = 0; i < second_hop.size(); i++)
    if (second_hop[i] < 0)
      return false;
  if (second_hop.size() == 0)
    return false;

  // combine configurations
  Vector<String> from_words, to_words;
  cp_argvec(from->configuration(), from_words);
  cp_argvec(to->configuration(), to_words);
  if (from_words.size() != first_hop.size()
      || to_words.size() != second_hop.size())
    return false;
  Vector<String> new_words;
  for (int i = 0; i < from_port; i++)
    new_words.push_back(from_words[i]);
  for (int i = 0; i < to_words.size(); i++)
    if (to_words[i] == "-")
      new_words.push_back(from_words[from_port]);
    else if (from_words[from_port] == "-")
      new_words.push_back(to_words[i]);
    else
      new_words.push_back(from_words[from_port] + " " + to_words[i]);
  for (int i = from_port + 1; i < from_words.size(); i++)
    new_words.push_back(from_words[i]);
  from->configuration() = cp_unargvec(new_words);

  // change connections
  router->kill_connection(first_hop[from_port]);
  for (int i = from_port + 1; i < first_hop.size(); i++)
    router->change_connection_from(first_hop[i], PortT(from, i + to_words.size() - 1));
  const Vector<ConnectionT> &conn = router->connections();
  for (int i = 0; i < second_hop.size(); i++)
    router->add_connection(PortT(from, from_port + i), conn[second_hop[i]].to());

  return true;
}

static bool
try_combine_classifiers(RouterT *router, ElementT *classifier)
{
  ElementClassT *classifier_t = router->get_type("Classifier");
  if (classifier->type() != classifier_t)
    // cannot combine IPClassifiers yet
    return false;

  Vector<PortT> branches;
  router->find_connections_to(PortT(classifier, 0), branches);
  for (int i = 0; i < branches.size(); i++)
    if (branches[i].elt->type() == classifier_t) {
      // perform a combination
      if (combine_classifiers(router, branches[i].elt, branches[i].port, classifier)) {
	try_combine_classifiers(router, classifier);
	return true;
      }
    }

  return false;
}

static void
try_remove_classifiers(RouterT *router, Vector<ElementT *> &classifiers)
{
  for (int i = 0; i < classifiers.size(); i++) {
    Vector<PortT> v;
    router->find_connections_to(PortT(classifiers[i], 0), v);
    if (v.size() == 0) {
      classifiers[i]->kill();
      classifiers[i] = classifiers.back();
      classifiers.pop_back();
      i--;
    }
  }

  router->remove_dead_elements();
  router->compact_connections();
}


/*
 * FastClassifier structures
 */

const String &
Classifier_Program::handler_value(const String &name) const
{
  for (int i = 0; i < handler_names.size(); i++)
    if (handler_names[i] == name)
      return handler_values[i];
  return String::null_string();
}

bool
operator!=(const Classifier_Insn &s1, const Classifier_Insn &s2)
{
  return (s1.yes != s2.yes
	  || s1.no != s2.no
	  || s1.offset != s2.offset
	  || s1.mask.u != s2.mask.u
	  || s1.value.u != s2.value.u);
}

bool
operator==(const Classifier_Insn &s1, const Classifier_Insn &s2)
{
  return !(s1 != s2);
}

bool
operator==(const Classifier_Program &c1, const Classifier_Program &c2)
{
  if (c1.type != c2.type
      || c1.safe_length != c2.safe_length
      || c1.output_everything != c2.output_everything
      || c1.noutputs != c2.noutputs
      || c1.align_offset != c2.align_offset
      || c1.program.size() != c2.program.size())
    return false;
  for (int i = 0; i < c1.program.size(); i++)
    if (c1.program[i] != c2.program[i])
      return false;
  if (c1.handler_names.size() != c2.handler_names.size())
    return false;
  for (int i = 0; i < c1.handler_names.size(); i++)
    if (c1.handler_values[i] != c2.handler_value(c1.handler_names[i]))
      return false;
  return true;
}

bool
operator!=(const Classifier_Program &c1, const Classifier_Program &c2)
{
  return !(c1 == c2);
}


/*
 * registering CIDs
 */

struct FastClassifier_Cid {
  String name;
  int guaranteed_packet_length;
  void (*headers)(const Classifier_Program &, StringAccum &);
  void (*checked_body)(const Classifier_Program &, StringAccum &);
  void (*unchecked_body)(const Classifier_Program &, StringAccum &);
  void (*push_body)(const Classifier_Program &, StringAccum &);
};

static HashMap<String, int> cid_name_map(-1);
static Vector<FastClassifier_Cid *> cids;
static Vector<String> interesting_handler_names;

int
add_classifier_type(const String &name, int guaranteed_packet_length,
	void (*headers)(const Classifier_Program &, StringAccum &),
	void (*checked_body)(const Classifier_Program &, StringAccum &),
	void (*unchecked_body)(const Classifier_Program &, StringAccum &),
	void (*push_body)(const Classifier_Program &, StringAccum &))
{
  FastClassifier_Cid *cid = new FastClassifier_Cid;
  cid->name = name;
  cid->guaranteed_packet_length = guaranteed_packet_length;
  cid->headers = headers;
  cid->checked_body = checked_body;
  cid->unchecked_body = unchecked_body;
  cid->push_body = push_body;
  cids.push_back(cid);
  cid_name_map.insert(cid->name, cids.size() - 1);
  return cids.size() - 1;
}

void
add_interesting_handler(const String &name)
{
  interesting_handler_names.push_back(name);
}


/*
 * translating Classifiers
 */

static String
translate_class_name(const String &s)
{
  StringAccum sa;
  for (int i = 0; i < s.length(); i++)
    if (s[i] == '_')
      sa << "_u";
    else if (s[i] == '@')
      sa << "_a";
    else if (s[i] == '/')
      sa << "_s";
    else
      sa << s[i];
  return sa.take_string();
}

static Vector<String> gen_eclass_names;
static Vector<String> gen_cxxclass_names;
static Vector<String> old_configurations;

static Vector<int> program_map;
static Vector<Classifier_Program> all_programs;

static void
change_landmark(ElementT *e)
{
  int colon = e->landmark().find_right(':');
  if (colon >= 0)
    e->set_landmark(e->landmark().substring(0, colon) + "<click-fastclassifier>" + e->landmark().substring(colon));
  else
    e->set_landmark(e->landmark() + "<click-fastclassifier>");
}

static void
copy_elements(RouterT *oldr, RouterT *newr, ElementClassT *type)
{
  if (type)
    for (RouterT::type_iterator x = oldr->begin_elements(type); x; x++)
      newr->get_element(x->name(), type, x->configuration(), "");
}

static void
analyze_classifiers(RouterT *r, const Vector<ElementT *> &classifiers,
		    ErrorHandler *errh)
{
  // set up new router
  RouterT nr;
  ElementT *idle = nr.add_anon_element(nr.get_type("Idle"));
  const Vector<String> &old_requirements = r->requirements();
  for (int i = 0; i < old_requirements.size(); i++)
    nr.add_requirement(old_requirements[i]);
  
  // copy AlignmentInfos and AddressInfos
  copy_elements(r, &nr, r->try_type("AlignmentInfo"));
  copy_elements(r, &nr, r->try_type("AddressInfo"));

  // copy all classifiers
  HashMap<String, int> classifier_map(-1);
  Vector<Classifier_Program> iprograms;
  for (int i = 0; i < classifiers.size(); i++) {
    ElementT *c = classifiers[i];
    classifier_map.insert(c->name(), i);
    
    // add new classifier and connections to idle
    ElementT *nc =
      nr.get_element(c->name(), c->type(), c->configuration(), c->landmark());
  
    nr.add_connection(idle, i, nc, 0);
    // count number of output ports
    int noutputs = c->noutputs();
    for (int j = 0; j < noutputs; j++)
      nr.add_connection(nc, j, idle, 0);

    // add program
    iprograms.push_back(Classifier_Program());
  }

  // read the relevant handlers from user-level `click'
  String handler_text;
  {
    StringAccum cmd_sa;
    cmd_sa << runclick_prog;
    for (int i = 0; i < interesting_handler_names.size(); i++)
      cmd_sa << " -h '*." << interesting_handler_names[i] << "'";
    cmd_sa << " -q";
    if (verbose)
      errh->message("Running command '%s' on configuration:\n%s", cmd_sa.c_str(), nr.configuration_string().c_str());
    handler_text = shell_command_output_string(cmd_sa.take_string(), nr.configuration_string(), errh);
  }

  // assign handlers to programs; assume handler results contain no par breaks
  {
    const char *s = handler_text.data();
    int len = handler_text.length();
    int pos = 0;
    String ename, hname, hvalue;
    while (pos < len) {
      // read element name
      int pos1 = pos;
      while (pos1 < len && s[pos1] != '.' && !isspace(s[pos1]))
	pos1++;
      ename = handler_text.substring(pos, pos1 - pos);
      bool ok = false;
      
      // read handler name
      if (pos1 < len && s[pos1] == '.') {
	pos1 = pos = pos1 + 1;
	while (pos1 < len && s[pos1] != ':' && !isspace(s[pos1]))
	  pos1++;
	hname = handler_text.substring(pos, pos1 - pos);
	
	// skip to EOL; data is good
	if (pos1 < len && s[pos1] == ':') {
	  for (pos1++; pos1 < len && s[pos1]!='\n' && s[pos1]!='\r'; pos1++)
	    /* nada */;
	  if (pos1 < len - 1 && s[pos1] == '\r' && s[pos1+1] == '\n')
	    pos1++;
	  pos1++;
	  ok = true;
	}
      }
      
      // skip to paragraph break
      int last_line_start = pos1;
      for (pos = pos1; pos1 < len; pos1++)
	if (s[pos1] == '\r' || s[pos1] == '\n') {
	  bool done = (pos1 == last_line_start);
	  if (pos1 < len - 1 && s[pos1] == '\r' && s[pos1+1] == '\n')
	    pos1++;
	  last_line_start = pos1 + 1; // loop will add 1 to pos1
	  if (done)
	    break;
	}
      hvalue = handler_text.substring(pos, pos1 - pos);

      // skip remaining whitespace
      for (pos = pos1; pos < len && isspace(s[pos]); pos++)
	/* nada */;

      // assign value to program if appropriate
      int prog_index = (ok ? classifier_map[ename] : -1);
      if (prog_index >= 0) {
	iprograms[prog_index].handler_names.push_back(hname);
	iprograms[prog_index].handler_values.push_back(hvalue);
      }
    }
  }

  // now parse each program
  for (int ci = 0; ci < iprograms.size(); ci++) {
    // check if valid handler
    String program = iprograms[ci].handler_value("program");
    if (!program) {
      program_map.push_back(-1);
      continue;
    }
    ElementT *c = classifiers[ci];

    // yes: valid handler; now parse program
    Classifier_Program &prog = iprograms[ci];
    String classifier_tname = c->type_name();
    prog.type = cid_name_map[classifier_tname];
    assert(prog.type >= 0);
    
    prog.safe_length = prog.output_everything = prog.align_offset = -1;
    prog.noutputs = c->noutputs();
    while (program) {
      // find step
      int newline = program.find_left('\n');
      String step = program.substring(0, newline);
      program = program.substring(newline + 1);
      // check for many things
      if (isdigit(step[0]) || isspace(step[0])) {
	// real step
	Classifier_Insn e;
	int crap, pos;
	int v[4], m[4];
	sscanf(step.c_str(), "%d %d/%2x%2x%2x%2x%%%2x%2x%2x%2x yes->%n",
	       &crap, &e.offset, &v[0], &v[1], &v[2], &v[3],
	       &m[0], &m[1], &m[2], &m[3], &pos);
	for (int i = 0; i < 4; i++) {
	  e.value.c[i] = v[i];
	  e.mask.c[i] = m[i];
	}
	// read yes destination
	step = step.substring(pos);
	if (step[0] == '[') {
	  sscanf(step.c_str(), "[%d] no->%n", &e.yes, &pos);
	  e.yes = -e.yes;
	} else
	  sscanf(step.c_str(), "step %d no->%n", &e.yes, &pos);
	// read no destination
	step = step.substring(pos);
	if (step[0] == '[') {
	  sscanf(step.c_str(), "[%d]", &e.no);
	  e.no = -e.no;
	} else
	  sscanf(step.c_str(), "step %d", &e.no);
	// push expr onto list
	prog.program.push_back(e);
      } else if (sscanf(step.c_str(), "all->[%d]", &prog.output_everything))
	/* nada */;
      else if (sscanf(step.c_str(), "safe length %d", &prog.safe_length))
	/* nada */;
      else if (sscanf(step.c_str(), "alignment offset %d", &prog.align_offset))
	/* nada */;
    }

    // search for an existing fast classifier with the same program
    bool found_program = false;
    for (int i = 0; i < all_programs.size() && !found_program; i++)
      if (prog == all_programs[i]) {
	program_map.push_back(i);
	found_program = true;
      }

    if (!found_program) {
      // set new names
      String class_name = "Fast" + classifier_tname + "@@" + c->name();
      String cxx_name = translate_class_name(class_name);
      prog.eclass = r->get_type(class_name);
      
      // add new program
      all_programs.push_back(prog);
      gen_eclass_names.push_back(class_name);
      gen_cxxclass_names.push_back(cxx_name);
      old_configurations.push_back(c->configuration());
      program_map.push_back(all_programs.size() - 1);
    }
  }

  // complain if any programs missing
  for (int i = 0; i < iprograms.size(); i++)
    if (program_map[i] < 0)
      errh->fatal("classifier program missing for `%s :: %s'!", classifiers[i]->name_cc(), classifiers[i]->type_name().cc());
}

static void
output_classifier_program(int which,
			  StringAccum &header, StringAccum &source,
			  ErrorHandler *)
{
  String cxx_name = gen_cxxclass_names[which];
  String class_name = gen_eclass_names[which];
  const Classifier_Program &prog = all_programs[which];
  FastClassifier_Cid *cid = cids[prog.type];
  if (cid->headers) {
    cid->headers(prog, source);
    cid->headers = 0;		// only call cid->headers once
  }
  
  header << "class " << cxx_name << " : public Element {\n\
  void devirtualize_all() { }\n\
 public:\n  "
	 << cxx_name << "() { set_ninputs(1); set_noutputs(" << prog.noutputs
	 << "); MOD_INC_USE_COUNT; }\n  ~"
	 << cxx_name << "() { MOD_DEC_USE_COUNT; }\n\
  const char *class_name() const { return \"" << class_name << "\"; }\n\
  " << cxx_name << " *clone() const { return new " << cxx_name << "; }\n\
  const char *processing() const { return PUSH; }\n";

  if (prog.output_everything >= 0) {
    header << "  void push(int, Packet *);\n};\n";
    source << "void\n" << cxx_name << "::push(int, Packet *p)\n{\n";
    if (prog.output_everything < prog.noutputs)
      source << "  output(" << prog.output_everything << ").push(p);\n";
    else
      source << "  p->kill();\n";
    source << "}\n";
  } else {
    bool need_checked = (prog.safe_length >= cid->guaranteed_packet_length);
    if (need_checked) {
      header << "  void length_checked_push(Packet *);\n";
      source << "void\n" << cxx_name << "::length_checked_push(Packet *p)\n{\n";
      cid->checked_body(prog, source);
      source << "}\n";
    }
    
    header << "  inline void length_unchecked_push(Packet *);\n\
  void push(int, Packet *);\n};\n";
    source << "inline void\n" << cxx_name
	   << "::length_unchecked_push(Packet *p)\n{\n";
    cid->unchecked_body(prog, source);
    source << "}\n";

    source << "void\n" << cxx_name << "::push(int, Packet *p)\n{\n";
    cid->push_body(prog, source);
    source << "}\n";
  }
}


static void
compile_classifiers(RouterT *r, const String &package_name,
		    Vector<ElementT *> &classifiers,
		    bool compile_kernel, bool compile_user, ErrorHandler *errh)
{
  // create C++ files
  StringAccum header, source;
  header << "#ifndef CLICK_" << package_name << "_HH\n"
	 << "#define CLICK_" << package_name << "_HH\n"
	 << "#include <click/package.hh>\n#include <click/element.hh>\n";
  source << "#include <click/config.h>\n\
#include \"" << package_name << ".hh\"\n\
#include <click/glue.hh>\n\
/** click-compile: -w */\n";

  // analyze Classifiers into programs
  analyze_classifiers(r, classifiers, errh);

  // add requirement
  r->add_requirement(package_name);

  // write Classifier programs
  for (int i = 0; i < all_programs.size(); i++)
    output_classifier_program(i, header, source, errh);

  // change element landmarks and types
  for (int i = 0; i < classifiers.size(); i++) {
    ElementT *classifier_e = classifiers[i];
    const Classifier_Program &prog = all_programs[program_map[i]];
    classifier_e->set_type(prog.eclass);
    classifier_e->set_configuration(String());
    change_landmark(classifier_e);
  }
  
  // write final text
  {
    header << "#endif\n";
    int nclasses = gen_cxxclass_names.size();
    source << "static int hatred_of_rebecca[" << nclasses << "];\n"
	   << "extern \"C\" int\ninit_module()\n{\n\
  click_provide(\""
	   << package_name << "\");\n";
    
    for (int i = 0; i < nclasses; i++)
      source << "  CLICK_DMALLOC_REG(\"FC" << i << "  \");\n"
	     << "  hatred_of_rebecca[" << i << "] = click_add_element_type(\""
	     << gen_eclass_names[i] << "\", new "
	     << gen_cxxclass_names[i] << ");\n";
    
    source << "  CLICK_DMALLOC_REG(\"FCxx\");\n  while (MOD_IN_USE > 1)\n    MOD_DEC_USE_COUNT;\n  return 0;\n}\n";
    source << "extern \"C\" void\ncleanup_module()\n{\n";
    for (int i = 0; i < nclasses; i++)
      source << "  click_remove_element_type(hatred_of_rebecca[" << i << "]);\n";
    source << "  click_unprovide(\"" << package_name << "\");\n}\n";
  }

  // compile files if required
  String tmpdir;
  
  if (compile_kernel || compile_user) {
    // create temporary directory
    if (!(tmpdir = click_mktmpdir(errh)))
      exit(1);
    
    String filename = tmpdir + package_name + ".hh";
    FILE *f = fopen(filename.c_str(), "w");
    if (!f)
      errh->fatal("%s: %s", filename.c_str(), strerror(errno));
    fwrite(header.data(), 1, header.length(), f);
    fclose(f);

    String cxx_filename = package_name + ".cc";
    f = fopen((tmpdir + cxx_filename).c_str(), "w");
    if (!f)
      errh->fatal("%s%s: %s", tmpdir.cc(), cxx_filename.cc(), strerror(errno));
    fwrite(source.data(), 1, source.length(), f);
    fclose(f);
    
    // compile kernel module
    if (compile_kernel) {
      String compile_command = click_compile_prog + " --directory=" + tmpdir + " --target=kernel --package=" + package_name + ".ko " + cxx_filename;
      int compile_retval = system(compile_command.cc());
      if (compile_retval == 127)
	errh->fatal("could not run `%s'", compile_command.cc());
      else if (compile_retval < 0)
	errh->fatal("could not run `%s': %s", compile_command.cc(), strerror(errno));
      else if (compile_retval != 0)
	errh->fatal("`%s' failed", compile_command.cc());
    }

    // compile userlevel
    if (compile_user) {
      String compile_command = click_compile_prog + " --directory=" + tmpdir + " --target=user --package=" + package_name + ".uo " + cxx_filename;
      int compile_retval = system(compile_command.cc());
      if (compile_retval == 127)
	errh->fatal("could not run `%s'", compile_command.cc());
      else if (compile_retval < 0)
	errh->fatal("could not run `%s': %s", compile_command.cc(), strerror(errno));
      else if (compile_retval != 0)
	errh->fatal("`%s' failed", compile_command.cc());
    }
  }

  // add .cc, .hh and .?o files to archive
  {
    ArchiveElement ae = init_archive_element(package_name + ".cc", 0600);
    ae.data = source.take_string();
    r->add_archive(ae);

    ae.name = package_name + ".hh";
    ae.data = header.take_string();
    r->add_archive(ae);

    if (compile_kernel) {
      ae.name = package_name + ".ko";
      ae.data = file_string(tmpdir + ae.name, errh);
      r->add_archive(ae);
    }
    
    if (compile_user) {
      ae.name = package_name + ".uo";
      ae.data = file_string(tmpdir + ae.name, errh);
      r->add_archive(ae);
    }
  }

  // add elementmap to archive
  {
    if (r->archive_index("elementmap-fastclassifier.xml") < 0)
      r->add_archive(init_archive_element("elementmap-fastclassifier.xml", 0600));
    ArchiveElement &ae = r->archive("elementmap-fastclassifier.xml");
    ElementMap em(ae.data);
    String header_file = package_name + ".hh";
    for (int i = 0; i < gen_eclass_names.size(); i++)
      em.add(gen_eclass_names[i], gen_cxxclass_names[i], header_file, "h/h", "x/x");
    ae.data = em.unparse("fastclassifier");
  }

  // add classifier configurations to archive
  {
    if (r->archive_index("fastclassifier_info") < 0)
      r->add_archive(init_archive_element("fastclassifier_info", 0600));
    ArchiveElement &ae = r->archive("fastclassifier_info");
    StringAccum sa;
    for (int i = 0; i < gen_eclass_names.size(); i++) {
      sa << gen_eclass_names[i] << '\t'
	 << cids[all_programs[i].type]->name << '\t'
	 << cp_quote(old_configurations[i]) << '\n';
    }
    ae.data += sa.take_string();
  }
}



static void
reverse_transformation(RouterT *r, ErrorHandler *)
{
  // parse fastclassifier_config
  if (r->archive_index("fastclassifier_info") < 0)
    return;
  ArchiveElement &fc_ae = r->archive("fastclassifier_info");
  Vector<String> click_names, old_type_names, configurations;
  parse_tabbed_lines(fc_ae.data, &click_names, &old_type_names,
		     &configurations, (void *)0);

  // prepare type_index_map : type_index -> configuration #
  HashMap<int, int> type_uid_map(-1);
  for (int i = 0; i < click_names.size(); i++) {
    int x = r->get_type(click_names[i])->uid();
    type_uid_map.insert(x, i);
  }

  // change configuration
  for (int i = 0; i < r->nelements(); i++) {
    ElementT *e = r->element(i);
    int x = type_uid_map[e->type_uid()];
    if (x >= 0) {
      e->set_configuration(configurations[x]);
      e->set_type(r->get_type(old_type_names[x]));
    }
  }

  // remove requirements
  {
    Vector<String> requirements = r->requirements();
    for (int i = 0; i < requirements.size(); i++)
      if (requirements[i].substring(0, 14) == "fastclassifier")
	r->remove_requirement(requirements[i]);
  }
  
  // remove archive elements
  for (int i = 0; i < r->narchive(); i++) {
    ArchiveElement &ae = r->archive(i);
    if (ae.name.substring(0, 14) == "fastclassifier"
	|| ae.name == "elementmap-fastclassifier.xml")
      ae.name = String();
  }
}

extern "C" {
void add_fast_classifiers_1();
void add_fast_classifiers_2();
}
  
int
main(int argc, char **argv)
{
  String::static_initialize();
  cp_va_static_initialize();
  ErrorHandler::static_initialize(new FileErrorHandler(stderr));
  ErrorHandler *errh = new PrefixErrorHandler
    (ErrorHandler::default_handler(), "click-fastclassifier: ");
  CLICK_DEFAULT_PROVIDES;

  // read command line arguments
  Clp_Parser *clp =
    Clp_NewParser(argc, argv, sizeof(options) / sizeof(options[0]), options);
  Clp_SetOptionChar(clp, '+', Clp_ShortNegated);
  program_name = Clp_ProgramName(clp);

  const char *router_file = 0;
  const char *output_file = 0;
  bool compile_kernel = false;
  bool compile_user = false;
  bool combine_classifiers = true;
  bool do_compile = true;
  bool source_only = false;
  bool config_only = false;
  bool reverse = false;
  bool quiet = false;
  bool file_is_expr = false;
  
  while (1) {
    int opt = Clp_Next(clp);
    switch (opt) {
      
     case HELP_OPT:
      usage();
      exit(0);
      break;
      
     case VERSION_OPT:
      printf("click-fastclassifier (Click) %s\n", CLICK_VERSION);
      printf("Copyright (c) 1999-2000 Massachusetts Institute of Technology\n\
Copyright (c) 2000-2001 Mazu Networks, Inc.\n\
Copyright (c) 2001 International Computer Science Institute\n\
This is free software; see the source for copying conditions.\n\
There is NO warranty, not even for merchantability or fitness for a\n\
particular purpose.\n");
      exit(0);
      break;

     case CLICKPATH_OPT:
      set_clickpath(clp->arg);
      break;
      
     case ROUTER_OPT:
     case EXPRESSION_OPT:
     case Clp_NotOption:
      if (router_file) {
	errh->error("router configuration specified twice");
	goto bad_option;
      }
      router_file = clp->arg;
      file_is_expr = (opt == EXPRESSION_OPT);
      break;

     case OUTPUT_OPT:
      if (output_file) {
	errh->error("output file specified twice");
	goto bad_option;
      }
      output_file = clp->arg;
      break;

     case COMBINE_OPT:
      combine_classifiers = !clp->negated;
      break;
      
     case COMPILE_OPT:
      do_compile = !clp->negated;
      break;
      
     case REVERSE_OPT:
      reverse = !clp->negated;
      break;
      
     case SOURCE_OPT:
      source_only = !clp->negated;
      break;
      
     case CONFIG_OPT:
      config_only = !clp->negated;
      break;
      
     case KERNEL_OPT:
      compile_kernel = !clp->negated;
      break;
      
     case USERLEVEL_OPT:
      compile_user = !clp->negated;
      break;

     case QUIET_OPT:
      quiet = !clp->negated;
      break;

     case VERBOSE_OPT:
      verbose = !clp->negated;
      break;

     bad_option:
     case Clp_BadOption:
      short_usage();
      exit(1);
      break;
      
     case Clp_Done:
      goto done;
      
    }
  }
  
 done:
  RouterT *r = read_router(router_file, file_is_expr, errh);
  if (r)
    r->flatten(errh);
  if (!r || errh->nerrors() > 0)
    exit(1);
  if (source_only || config_only)
    compile_user = compile_kernel = false;

  // open output file
  FILE *outf = stdout;
  if (output_file && strcmp(output_file, "-") != 0) {
    outf = fopen(output_file, "w");
    if (!outf)
      errh->fatal("%s: %s", output_file, strerror(errno));
  }

  // handle reverse case
  if (reverse) {
    reverse_transformation(r, errh);
    write_router_file(r, outf, errh);
    exit(0);
  }

  // install classifier handlers
  add_interesting_handler("program");
  add_fast_classifiers_1();
  add_fast_classifiers_2();
  
  // find Click binaries
  runclick_prog = clickpath_find_file("click", "bin", CLICK_BINDIR, errh);
  click_compile_prog = clickpath_find_file("click-compile", "bin", CLICK_BINDIR, errh);
  if (quiet)
    click_compile_prog += " -q";

  // find Classifiers
  Vector<ElementT *> classifiers;
  for (RouterT::iterator x = r->begin_elements(); x; x++)
    if (cid_name_map[x->type_name()] >= 0)
      classifiers.push_back(x);

  // quit early if no Classifiers
  if (classifiers.size() == 0) {
    if (source_only)
      errh->message("no Classifiers in router");
    else
      write_router_file(r, outf, errh);
    exit(0);
  }

  // find name of package
  String package_name = "fastclassifier";
  int uniqueifier = 1;
  while (1) {
    if (r->archive_index(package_name + ".cc") < 0)
      break;
    uniqueifier++;
    package_name = "fastclassifier" + String(uniqueifier);
  }
  
  // try combining classifiers
  if (combine_classifiers) {
    bool any_combined = false;
    for (int i = 0; i < classifiers.size(); i++)
      any_combined |= try_combine_classifiers(r, classifiers[i]);
    if (any_combined)
      try_remove_classifiers(r, classifiers);
  }

  if (do_compile)
    compile_classifiers(r, package_name, classifiers,
			compile_kernel, compile_user, errh);

  // write output
  if (source_only) {
    if (r->archive_index(package_name + ".hh") < 0) {
      errh->error("no source code generated");
      exit(1);
    }
    const ArchiveElement &aeh = r->archive(package_name + ".hh");
    const ArchiveElement &aec = r->archive(package_name + ".cc");
    fwrite(aeh.data.data(), 1, aeh.data.length(), outf);
    fwrite(aec.data.data(), 1, aec.data.length(), outf);
  } else if (config_only) {
    String config = r->configuration_string();
    fwrite(config.data(), 1, config.length(), outf);
  } else
    write_router_file(r, outf, errh);
  
  exit(0);
}

// generate Vector template instance
#include <click/vector.cc>
template class Vector<Classifier_Insn>;
template class Vector<Classifier_Program>;
