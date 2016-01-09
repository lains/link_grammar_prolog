These instructions show examples on how to use the Link Grammar from SWI-Prolog.

First, load the library.
Open the SWI-Prolog interprete and run the following commands:
?- use_module(library(lgp)).

%  library(shlib) compiled into shlib 0.01 sec, 9,176 bytes
% c:/program files/swi-prolog/lgp_lib.pl compiled into lgp_lib 0.01 sec, 28,172 bytes

?- lgp:create_dictionary('4.0.dict', '4.0.knowledge', '4.0.constituent-knowledge', '4.0.affix', Dictionary_handle).

Dictionary_handle = '$dictionary'(0)

Yes
% Note: If an exception "cant_create" is reported, make sure that the words/ subdirectory is present in the current directory, and that the 4 specified database files (dict, knowledge, constituent-knowledge and affix) are also referenced properly from the current directory.
% If SWI-Prolog is killed during this command, check that the content of words/ is matching with the LGP source files used to build lgp.dll (even if the version appears to be the same, it is always better to reextract the content of words/ when extracting the system-?.?.tar.gz archive


?- lgp:create_parse_options([disjunct_cost=2, min_null_count=0, max_null_count=0, linkage_limit=100, max_parse_time=10, max_memory=128000000], PO_handle).

PO_handle = '$options'(0) 

Yes
?- lgp:create_parse_options([disjunct_cost=3, min_null_count=1, max_null_count=250, linkage_limit=100, max_parse_time=10, max_memory=128000000], Panic_PO_handle).

Panic_PO_handle = '$options'(1) 

Yes
?- lgp:enable_panic_on_parse_options($Panic_PO_handle).

Yes
?- lgp:create_sentence('This is the first recorded sentence', $Dictionary_handle, Sentence_0).

Sentence_0 = '$sentence'(0) 

Yes
?- lgp:create_linkage_set($Sentence_0, $PO_handle, Linkage_set_handle_0).

Linkage_set_handle_0 = '$linkageset'(0) 

Yes
?- lgp:create_sentence('The software is now fully installed', $Dictionary_handle, Sentence_1).

Sentence_1 = '$sentence'(1) 

Yes
?- lgp:create_linkage_set($Sentence_1, $PO_handle, Linkage_set_handle_1).

Linkage_set_handle_1 = '$linkageset'(1) 

Yes
?- get_nb_parse_options(Nb).

Nb = 2 

Yes
?- get_handles_nb_references_parse_options(Handle, Nb).

Handle = ['$options'(1), '$options'(0)]
Nb = [0, 2] 

Yes
?- get_nb_sentences(Nb).

Nb = 2 ;

No
?- get_handles_nb_references_sentences(Handle, Nb).

Handle = ['$sentence'(1), '$sentence'(0)]
Nb = [1, 1] 

Yes
?- get_handles_sentences(HS).

HS = ['$sentence'(1), '$sentence'(0)] 

Yes
?- lgp:get_linkage($Linkage_set_handle_1, Linkage).
Linkage = [link([m], connection(e-[m], fully(_G608), installed(v))), link([m], connection(e-[], now(e), installed(v))), link([m], connection(p-[v], is(v), installed(v))), link([m], connection(s-[s], software(n), is(v))), link([m], connection(d-[m|...], the(_G516), software(n))), link([m], connection(w-[...], 'left-wall'(_G493), software(n))), link([], connection(... -..., 'left-wall'(...), 'right-wall'(...)))] ;

No
% This is the list of links in this linkage set. SWI-Prolog doesn't print this list in a readable way, so the following will extract one member after the other.
?- help(member).
member(?Elem, ?List)
    Succeeds  when Elem can be unified with one of the members  of List.
    The predicate can be used with any instantiation pattern.


Yes
?- member(One_link, $Linkage).

One_link = link([m], connection(e-[m], fully(_G355), installed(v))) ;

One_link = link([m], connection(e-[], now(e), installed(v))) ;

One_link = link([m], connection(p-[v], is(v), installed(v))) ;

One_link = link([m], connection(s-[s], software(n), is(v))) ;

One_link = link([m], connection(d-[m, u], the(_G447), software(n))) ;

One_link = link([m], connection(w-[d], 'left-wall'(_G470), software(n))) ;

One_link = link([], connection(rw-[], 'left-wall'(_G487), 'right-wall'(_G489))) ;

No
% We now go back the the sentence 0, recorded in linkage set 0, and fail get_linkage to see the 3 different linkages in the linkage set
?- lgp:get_linkage($Linkage_set_handle_0, Linkage).

Linkage = [link([m], connection(e-[], first(a), recorded(v))), link([m], connection(a-[], recorded(v), sentence(n))), link([m], connection(d-[s], the(_G622), sentence(n))), link([m], connection(o-[s, t], is(v), sentence(n))), link([m], connection(s-[s|...], this(p), is(v))), link([m], connection(w-[...], 'left-wall'(_G543), this(p))), link([], connection(... -..., 'left-wall'(...), 'right-wall'(...)))] ;

Linkage = [link([m], connection(a-[], recorded(v), sentence(n))), link([m], connection(l-[], the(_G645), first(a))), link([m], connection(d-[s], the(_G622), sentence(n))), link([m], connection(o-[s, t], is(v), sentence(n))), link([m], connection(s-[s|...], this(p), is(v))), link([m], connection(w-[...], 'left-wall'(_G543), this(p))), link([], connection(... -..., 'left-wall'(...), 'right-wall'(...)))] ;

Linkage = [link([m], connection(a-[], recorded(v), sentence(n))), link([m], connection(an-[], first(n), sentence(n))), link([m], connection(d-[s], the(_G622), sentence(n))), link([m], connection(o-[s, t], is(v), sentence(n))), link([m], connection(s-[s|...], this(p), is(v))), link([m], connection(w-[...], 'left-wall'(_G543), this(p))), link([], connection(... -..., 'left-wall'(...), 'right-wall'(...)))] ;

No
% Let's now look at the last (third) possible linkage set, which is the last one returned by get_linkage/2 before failing
?- member(One_link, $Linkage).

One_link = link([m], connection(a-[], recorded(v), sentence(n))) ;

One_link = link([m], connection(an-[], first(n), sentence(n))) ;

One_link = link([m], connection(d-[s], the(_G403), sentence(n))) ;

One_link = link([m], connection(o-[s, t], is(v), sentence(n))) ;

One_link = link([m], connection(s-[s, _G452, b], this(p), is(v))) ;

One_link = link([m], connection(w-[d], 'left-wall'(_G481), this(p))) ;

One_link = link([], connection(rw-[], 'left-wall'(_G498), 'right-wall'(_G500))) ;

No
% The real power of the combination of the Prolog engine and the Link Grammar Parser is the ability to look for a specific link in the sentence. Prolog will thus find the linkage that matches the pattern searched for, within the whole linkage set:
?- lgp:get_linkage($Linkage_set_handle_0, Linkage), member(One_link, Linkage), One_link=link([_], connection(Link_type-Link_subtype_list, is(Type_a), Word_b)).

Linkage = [link([m], connection(e-[], first(a), recorded(v))), link([m], connection(a-[], recorded(v), sentence(n))), link([m], connection(d-[s], the(_G1426), sentence(n))), link([m], connection(o-[s, t], is(v), sentence(n))), link([m], connection(s-[s|...], this(p), is(v))), link([m], connection(w-[...], 'left-wall'(_G1347), this(p))), link([], connection(... -..., 'left-wall'(...), 'right-wall'(...)))]
One_link = link([m], connection(o-[s, t], is(v), sentence(n)))
Link_type = o
Link_subtype_list = [s, t]
Type_a = v
Word_b = sentence(n) ;

Linkage = [link([m], connection(a-[], recorded(v), sentence(n))), link([m], connection(l-[], the(_G1449), first(a))), link([m], connection(d-[s], the(_G1426), sentence(n))), link([m], connection(o-[s, t], is(v), sentence(n))), link([m], connection(s-[s|...], this(p), is(v))), link([m], connection(w-[...], 'left-wall'(_G1347), this(p))), link([], connection(... -..., 'left-wall'(...), 'right-wall'(...)))]
One_link = link([m], connection(o-[s, t], is(v), sentence(n)))
Link_type = o
Link_subtype_list = [s, t]
Type_a = v
Word_b = sentence(n) ;

Linkage = [link([m], connection(a-[], recorded(v), sentence(n))), link([m], connection(an-[], first(n), sentence(n))), link([m], connection(d-[s], the(_G1426), sentence(n))), link([m], connection(o-[s, t], is(v), sentence(n))), link([m], connection(s-[s|...], this(p), is(v))), link([m], connection(w-[...], 'left-wall'(_G1347), this(p))), link([], connection(... -..., 'left-wall'(...), 'right-wall'(...)))]
One_link = link([m], connection(o-[s, t], is(v), sentence(n)))
Link_type = o
Link_subtype_list = [s, t]
Type_a = v
Word_b = sentence(n) ;

No
% The expression above looks for all different links concerning is, and returns the linkage (Linkage), the link (One_link), the type of the link (Link_type) and its subtype if any (Link_subtype_list), as well as the type found for the word 'is' (Type_a) and the word link to 'is' (in Word_b)
% We can also look for all objects linked with the:
?- lgp:get_linkage($Linkage_set_handle_0, Linkage), member(One_link, Linkage), One_link=link([_], connection(_-_, the(_Type_a), Word_b)).

Linkage = [link([m], connection(e-[], first(a), recorded(v))), link([m], connection(a-[], recorded(v), sentence(n))), link([m], connection(d-[s], the(_G982), sentence(n))), link([m], connection(o-[s, t], is(v), sentence(n))), link([m], connection(s-[s|...], this(p), is(v))), link([m], connection(w-[...], 'left-wall'(_G1185), this(p))), link([], connection(... -..., 'left-wall'(...), 'right-wall'(...)))]
One_link = link([m], connection(d-[s], the(_G982), sentence(n)))
Word_b = sentence(n) ;

Linkage = [link([m], connection(a-[], recorded(v), sentence(n))), link([m], connection(l-[], the(_G982), first(a))), link([m], connection(d-[s], the(_G1264), sentence(n))), link([m], connection(o-[s, t], is(v), sentence(n))), link([m], connection(s-[s|...], this(p), is(v))), link([m], connection(w-[...], 'left-wall'(_G1185), this(p))), link([], connection(... -..., 'left-wall'(...), 'right-wall'(...)))]
One_link = link([m], connection(l-[], the(_G982), first(a)))
Word_b = first(a) ;

Linkage = [link([m], connection(a-[], recorded(v), sentence(n))), link([m], connection(l-[], the(_G1287), first(a))), link([m], connection(d-[s], the(_G982), sentence(n))), link([m], connection(o-[s, t], is(v), sentence(n))), link([m], connection(s-[s|...], this(p), is(v))), link([m], connection(w-[...], 'left-wall'(_G1185), this(p))), link([], connection(... -..., 'left-wall'(...), 'right-wall'(...)))]
One_link = link([m], connection(d-[s], the(_G982), sentence(n)))
Word_b = sentence(n) ;

Linkage = [link([m], connection(a-[], recorded(v), sentence(n))), link([m], connection(an-[], first(n), sentence(n))), link([m], connection(d-[s], the(_G982), sentence(n))), link([m], connection(o-[s, t], is(v), sentence(n))), link([m], connection(s-[s|...], this(p), is(v))), link([m], connection(w-[...], 'left-wall'(_G1185), this(p))), link([], connection(... -..., 'left-wall'(...), 'right-wall'(...)))]
One_link = link([m], connection(d-[s], the(_G982), sentence(n)))
Word_b = sentence(n) ;

No
% Now, let's have a look at the linkage set objects existing in the memory:
?- get_handles_linkage_sets(HLS).

HLS = ['$linkageset'(1), '$linkageset'(0)] 

Yes
% We can consult each linkage set to know how many linkages it contains, which sentence it has been created from, and which parse options it used:
?- get_full_info_linkage_sets($linkageset'(1), PLS1).

PLS1 = [num_linkage=1, sentence_handle='$sentence'(1), parse_options_handle='$options'(0)] 

Yes
% An other solution is to get the information about all linkage sets using get_full_info_linkage_sets/1
?- get_full_info_linkage_sets(Full_info).

Full_info = [[handle='$linkageset'(1), num_linkage=1, sentence_handle='$sentence'(1), parse_options_handle='$options'(0)], [handle='$linkageset'(0), num_linkage=3, sentence_handle='$sentence'(0), parse_options_handle='$options'(0)]] 

Yes
% The following checks that the reference counts are updated when freeing up objects
?- get_handles_nb_references_sentences(Handle, Nb).

Handle = ['$sentence'(1), '$sentence'(0)]
Nb = [1, 1] 

Yes
?- delete_linkage_set($Linkage_set_handle_0).

Yes
% After linkage set 0 has been removed, sentence 0 is not referenced anymore
?- get_handles_nb_references_sentences(Handle, Nb).

Handle = ['$sentence'(1), '$sentence'(0)]
Nb = [1, 0] 

Yes
% We can't delete sentence 1 because it is still referenced
?- delete_sentence('$sentence'(1)).
ERROR: Unhandled exception: lgp_api_error(sentence, still_referenced)

% However, we can now delete sentence 0
?- delete_sentence('$sentence'(0)).

Yes
% Dictionary 1 is now only referenced by sentence 1 (by only one object)
?- get_handles_nb_references_dictionaries(Handle, Nb).

Handle = ['$dictionary'(1)]
Nb = [1] 

Yes
?- delete_all_sentences.

Yes
% Returns yes, but actually failed because of one sentence '$sentence'(1) that is still referenced

% Let's free up the memory by getting rid of the linkage sets, sentences and parse options
?- delete_all_linkage_sets.

Yes
?- get_full_info_linkage_sets(Full_info).

Full_info = [] 

Yes
?- delete_all_sentences.

Yes
?- get_nb_sentences(Nb).

Nb = 0 

Yes
?- delete_all_parse_options.

Yes
?- get_handles_parse_options(HPO).

HPO = [] 

Yes
% We now only have a dictionary object in memory:
?- get_handles_dictionaries(HD).

HD = ['$dictionary'(0)] 

Yes
% Let's delete it as well
?- delete_dictionary('$dictionary'(0)).

Yes
?- get_handles_dictionaries(HD).

HD = [] 

Yes
% Note: when all dictionaries have been deleted, an automatic memory leak check is performed
% In case some memory could not be deallocated properly, an exception "lgp_api(external_space, leak)" will be raised.

% We now query Prolog to know which are the foreign libraries loaded
?- current_foreign_library(Lib, Predicates).

Lib = lgp
Predicates = [lgp:create_dictionary(_G454, _G455, _G456, _G457, _G458), lgp:delete_dictionary(_G466), lgp:delete_all_dictionaries, lgp:get_nb_dictionaries(_G480), lgp:get_handles_dictionaries(_G488), lgp:create_parse_options_(_G496), lgp:delete_parse_options(_G504), lgp:delete_all_parse_options, ... :...|...]

Yes
% Let's unload the lgp_lib from the memory (this will deallocate the whole space taken by the DLL)
?- unload_foreign_library(lgp).

Yes
% We can double-check that the lgp library is not in memory anymore:
?- current_foreign_library(Lib, Predicates).

No
?- 
