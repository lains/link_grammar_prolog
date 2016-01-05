:- use_module(lgp_lib).

go:-
	set_prolog_flag(failure_number, 0),
	fail.
go:-
	scheduled_test_name(Family),
	write('Starting family of tests: '), write_ln(Family),
	set_prolog_flag(test_level, 1),
	(   catch(
		  go(Family),
		  Exception_raised,
		  (   write_ln('***'),
		      format('*** Exception ''~w'' raised in family ~w~n', [Exception_raised, Family]),
		      current_prolog_flag(failure_number, F_N),
		      New_F_N is F_N + 1,
		      set_prolog_flag(failure_number, New_F_N)
		  )
		 )
	->  true
	;   write_ln('***'),
	    format('*** Failure found in family ~w~n', Family)
	),
	write_ln('***'),
	current_prolog_flag(failure_number, F_N),
	format('*** Total number of level failed so far: ~w~n', F_N),
	write_ln('***'),

	fail.		    % Fail to force redo scheduled_test_name/1
go.
go(Family):-
	scheduled_test_name(Family, Name, Parms),
	current_prolog_flag(test_level, Current_level),
	format('~w - Running test "~w:~w" ~w...~n', [Current_level, Family, Name, Parms]),
	Next_level is Current_level + 1,
	set_prolog_flag(test_level, Next_level), % Set test level to one level deeper
	(   execute_test_name(Family, Name, Parms, '')
	->  format('~w - ... passed~n', [Current_level, Family, Name])
	;   format('~w - ... ###FAILED###~n', [Current_level, Family, Name]),
	    current_prolog_flag(failure_number, F_N),
	    New_F_N is F_N + 1,
	    set_prolog_flag(failure_number, New_F_N),
	    !, trace, fail
	),
	fail.
go(_).
go(Family, Name, Parms, Indent):-
	!,
	atom_concat(Indent, '  ', New_indent),
	current_prolog_flag(test_level, Current_level),
	format('~w - ~wRunning test "~w:~w" ~w...~n', [Current_level, New_indent, Family, Name, Parms]),
	Next_level is Current_level + 1,
	set_prolog_flag(test_level, Next_level), % Set test level to one level deeper
	(   execute_test_name(Family, Name, Parms, New_indent)
	->  format('~w - ~w... passed~n', [Current_level, New_indent, Family, Name])
	;   format('~w - ~w... ###FAILED###~n', [Current_level, New_indent, Family, Name]),
	    current_prolog_flag(failure_number, F_N),
	    New_F_N is F_N + 1,
	    set_prolog_flag(failure_number, New_F_N),
	    !, trace, fail
	
	).

create_parms_dictionary(['4.0.dict', '4.0.knowledge', '4.0.constituent-knowledge', '4.0.affix']).
create_parms_sentence_unique_linkage('The software is now fully installed').
create_parms_sentence_multiple_linkages('This is the first recorded sentence', 3).
create_parms_parse_options_normal([disjunct_cost=2, min_null_count=0, max_null_count=0, linkage_limit=100, max_parse_time=10, max_memory=128000000]).
create_parms_parse_options_panic([disjunct_cost=3, min_null_count=1, max_null_count=250, linkage_limit=100, max_parse_time=10, max_memory=128000000]).

:- discontiguous scheduled_test_name/1.
:- discontiguous scheduled_test_name/3.
	
scheduled_test_name('Cleanup').
scheduled_test_name('Normal use').
scheduled_test_name('Cleanup').
scheduled_test_name('Dictionary').
scheduled_test_name('Parse Options').
scheduled_test_name('Sentence').
scheduled_test_name('Linkage Set').
scheduled_test_name('Cleanup').
%scheduled_test_name('Foreign').

scheduled_test_name('Cleanup', 'deletion of all objects', []).
scheduled_test_name('Normal use', 'get linkage for one sentence', [create_parms_dict=Create_parms_dict,
								   create_parms_sent=Create_parms_sent,
								   create_parms_opts=Create_parms_opts,
								   create_parms_opts_panic=Create_parms_opts_panic,
								   num_linkage_expected=Number_linkage]):-
	create_parms_dictionary(Create_parms_dict),
	create_parms_sentence_multiple_linkages(Create_parms_sent, Number_linkage),
	create_parms_parse_options_normal(Create_parms_opts),
	create_parms_parse_options_panic(Create_parms_opts_panic).
scheduled_test_name('Normal use', 'sample creation of two linkage sets', [create_parms_dict=Create_parms_dict,
									  create_parms_sent1=Create_parms_sent1,
									  create_parms_sent2=Create_parms_sent2,
									  create_parms_opts=Create_parms_opts,
									  create_parms_opts_panic=Create_parms_opts_panic,
									  num_linkage_expected=Number_linkage]):-
	create_parms_dictionary(Create_parms_dict),
	create_parms_sentence_unique_linkage(Create_parms_sent1),
	create_parms_sentence_multiple_linkages(Create_parms_sent2, Number_linkage),
	create_parms_parse_options_normal(Create_parms_opts),
	create_parms_parse_options_panic(Create_parms_opts_panic).

scheduled_test_name('Foreign', 'unload/reload', []):-
	unload_foreign_library(lgp),
	use_module(lgp_lib).

scheduled_test_name('Dictionary', 'creation/deletion', [create_parms_dict=Create_parms_dict]):-
	create_parms_dictionary(Create_parms_dict).
scheduled_test_name('Dictionary', 'addition of a reference', [create_parms_dict=Create_parms_dict,
							      create_parms_sent=Create_parms_sent]):-
	create_parms_dictionary(Create_parms_dict),
	create_parms_sentence_unique_linkage(Create_parms_sent).
scheduled_test_name('Dictionary', 'deletion of non-existing handle', [create_parms_dict=Create_parms_dict,
								      handle('Dictionary')=_Handle_dict]):-
	create_parms_dictionary(Create_parms_dict).

scheduled_test_name('Parse Options', 'creation/deletion', [create_parms_opts=Create_parms_opts]):-
	create_parms_parse_options_normal(Create_parms_opts).
scheduled_test_name('Parse Options', 'addition of a reference', [create_parms_dict=Create_parms_dict,
								 create_parms_sent=Create_parms_sent,
								 create_parms_opts=Create_parms_opts]):-
	create_parms_dictionary(Create_parms_dict),
	create_parms_sentence_unique_linkage(Create_parms_sent),
	create_parms_parse_options_normal(Create_parms_opts).
scheduled_test_name('Parse Options', 'deletion of non-existing handle', [create_parms_opts=Create_parms_opts,
									 handle('Parse Options')=_Handle_opts]):-
	create_parms_parse_options_normal(Create_parms_opts).

scheduled_test_name('Sentence', 'creation/deletion', [create_parms_dict=Create_parms_dict,
						      create_parms_sent=Create_parms_sent,
						      handle('Sentence')=_Handle_sent,
						      handle('Dictionary')=_Handle_dict]):-
	create_parms_dictionary(Create_parms_dict),
	create_parms_sentence_unique_linkage(Create_parms_sent).
scheduled_test_name('Sentence', 'addition of a reference', [create_parms_dict=Create_parms_dict,
							    create_parms_sent=Create_parms_sent,
							    create_parms_opts=Create_parms_opts]):-
	create_parms_dictionary(Create_parms_dict),
	create_parms_sentence_unique_linkage(Create_parms_sent),
	create_parms_parse_options_normal(Create_parms_opts).
scheduled_test_name('Sentence', 'deletion of non-existing handle', [create_parms_dict=Create_parms_dict,
								    create_parms_sent=Create_parms_sent,
								    handle('Dictionary')=_Handle_dict,
								    handle('Sentence')=_Handle_sent]):-
	create_parms_dictionary(Create_parms_dict),
	create_parms_sentence_unique_linkage(Create_parms_sent).

scheduled_test_name('Linkage Set', 'creation/deletion', [create_parms_dict=Create_parms_dict,
							 create_parms_sent=Create_parms_sent,
							 create_parms_opts=Create_parms_opts,
							 handle('Dictionary')=_Handle_dict,
							 handle('Sentence')=_Handle_sent,
							 handle('Parse Options')=_Handle_opts]):-
	create_parms_dictionary(Create_parms_dict),
	create_parms_sentence_unique_linkage(Create_parms_sent),
	create_parms_parse_options_normal(Create_parms_opts).
scheduled_test_name('Linkage Set', 'deletion of non-existing handle',
		    [create_parms_dict=Create_parms_dict,
		     create_parms_sent=Create_parms_sent,
		     create_parms_opts=Create_parms_opts,
		     handle('Dictionary')=_Handle_dict,
		     handle('Sentence')=_Handle_sent,
		     handle('Parse Options')=_Handle_opts,
		     handle('Linkage Set')=_Handle_link]):-
	create_parms_dictionary(Create_parms_dict),
	create_parms_sentence_unique_linkage(Create_parms_sent),
	create_parms_parse_options_normal(Create_parms_opts).
%scheduled_test_name('Linkage Set', 'deletion of linkage set object before cut of get_linkage', 
%		  [create_parms_dict=Create_parms_dict,
%		   create_parms_sent=Create_parms_sent,
%		   create_parms_opts=Create_parms_opts,
%		   handle('Dictionary')=_Handle_dict,
%		   handle('Sentence')=_Handle_sent,
%		   handle('Parse Options')=_Handle_opts,
%		   handle('Linkage Set')=_Handle_link]):-
%	create_parms_dictionary(Create_parms_dict),
%	create_parms_sentence_multiple_linkages(Create_parms_sent, _Number_linkage),
%	create_parms_parse_options_normal(Create_parms_opts).

%scheduled_test_name('Dictionary', 'multiple creation/deletion', [base=dictionary]).








object_list_type_singular('Dictionary', dictionary).
object_list_type_singular('Sentence', sentence).
object_list_type_singular('Parse Options', parse_options).
object_list_type_singular('Linkage Set', linkage_set).
object_list_type_plural('Dictionary', dictionaries).
object_list_type_plural('Sentence', sentences).
object_list_type_plural('Parse Options', parse_options).
object_list_type_plural('Linkage Set', linkage_sets).










execute_test_name('Cleanup', 'deletion of all objects', _Parms, Indent):-
	!,
	go('Linkage Set', 'deletion of the whole list', [], Indent),
	go('Sentence', 'deletion of the whole list', [], Indent),
	go('Parse Options', 'deletion of the whole list', [], Indent),
	go('Dictionary', 'deletion of the whole list', [], Indent).


execute_test_name('Normal use', 'get linkage for one sentence', Parms, Indent):-
	!,
	member(create_parms_dict=Create_parm_dict, Parms),
	member(create_parms_sent=Create_parm_sent, Parms),
	member(create_parms_opts=Create_parm_opts, Parms),
	member(create_parms_opts_panic=Create_parm_opts_panic, Parms),
	go('Dictionary', 'creation of one object', [create_parms=Create_parm_dict, handle=Handle_dict], Indent),
	go('Sentence', 'creation of one object', [create_parms=[Create_parm_sent, Handle_dict], handle=Handle_sent], Indent),
	go('Parse Options', 'creation of one object', [create_parms=[Create_parm_opts], handle=Handle_opts], Indent),
	go('Parse Options', 'creation of one object', [create_parms=[Create_parm_opts_panic], handle=Handle_opts_panic], Indent),
	lgp_lib:enable_panic_on_parse_options(Handle_opts_panic),
	go('Linkage Set', 'creation of one object', [create_parms=[Handle_sent, Handle_opts], handle=Handle_link], Indent),
	findall(One_link, lgp_lib:get_linkage(Handle_link, One_link), List_links),
	length(List_links, Number_of_linkages),
	member(num_linkage_expected=Number_of_linkages, Parms).

execute_test_name('Normal use', 'sample creation of two linkage sets', Parms, Indent):-
	!,
	get_nb_parse_options(Nb_parse_options_original),
	get_nb_sentences(Nb_sentences_original),
	member(create_parms_dict=Create_parm_dict, Parms),
	member(create_parms_sent1=Create_parm_sent1, Parms),
	member(create_parms_sent2=Create_parm_sent2, Parms),
	member(create_parms_opts=Create_parm_opts, Parms),
	member(create_parms_opts_panic=Create_parm_opts_panic, Parms),
	member(num_linkage_expected=Number_linkage_for_link2, Parms),
go('Dictionary', 'creation of one object', [create_parms=Create_parm_dict, handle=Handle_dict], Indent),
	go('Parse Options', 'creation of one object', [create_parms=[Create_parm_opts], handle=Handle_opts], Indent),
	go('Parse Options', 'creation of one object', [create_parms=[Create_parm_opts_panic], handle=Handle_opts_panic], Indent),
	lgp_lib:enable_panic_on_parse_options(Handle_opts_panic),	
	go('Sentence', 'creation of one object', [create_parms=[Create_parm_sent1, Handle_dict], handle=Handle_sent1], Indent),
	go('Linkage Set', 'creation of one object', [create_parms=[Handle_sent1, Handle_opts], handle=Handle_link1], Indent),
	go('Sentence', 'creation of one object', [create_parms=[Create_parm_sent2, Handle_dict], handle=Handle_sent2], Indent),
	go('Linkage Set', 'creation of one object', [create_parms=[Handle_sent2, Handle_opts], handle=Handle_link2], Indent),
	Nb_parse_options_expected is Nb_parse_options_original + 2,
	(   get_nb_parse_options(Nb_parse_options_expected)
	->  true
	;   throw(test_fail, 'Incorrect number of parse options')
	),
	Nb_sentences_expected is Nb_sentences_original + 2,
	go('Parse Options', 'verification of reference count', [handle=Handle_opts, ref_count=2], Indent),
	go('Parse Options', 'verification of reference count', [handle=Handle_opts_panic, ref_count=0], Indent),
	get_nb_sentences(Nb_sentences_expected),
	go('Sentence', 'verification of reference count', [handle=Handle_sent1, ref_count=1], Indent),
	go('Sentence', 'verification of reference count', [handle=Handle_sent2, ref_count=1], Indent),
	get_handles_sentences(Handles_sentences),
	memberchk(Handle_sent1, Handles_sentences),
	memberchk(Handle_sent2, Handles_sentences),
	(   get_linkage(Handle_link1, Linkage1),
	    member(One_link1, Linkage1),
	    format('~wLink found in 1 ~w~n', [Indent, One_link1]),
	    fail
	;   true
	),
	(   get_linkage(Handle_link2, Linkage2),
	    member(One_link2, Linkage2),
	    format('~wLink found in 2 ~w~n', [Indent, One_link2]),
	    fail
	;   true
	),
	get_handles_linkage_sets(Handles_linkage_sets),
	memberchk(Handle_link1, Handles_linkage_sets),
	memberchk(Handle_link2, Handles_linkage_sets),
	get_parameters_for_linkage_set(Handle_link2, Param_ls2),
	memberchk(num_linkage=Number_linkage_for_link2, Param_ls2),
	memberchk(sentence_handle=Handle_sent2, Param_ls2),
	memberchk(parse_options_handle=Handle_opts, Param_ls2),
	get_full_info_linkage_sets(Full_info),
	member(One_info1, Full_info),
	member(handle=Handle_link1, One_info1),
	member(One_info2, Full_info),
	member(handle=Handle_link2, One_info2),
	member(num_linkage=Number_linkage_for_link2, One_info2),
	member(sentence_handle=Handle_sent2, One_info2),
	member(parse_options_handle=Handle_opts, One_info2),
	delete_linkage_set(Handle_link2),
	go('Sentence', 'verification of reference count', [handle=Handle_sent1, ref_count=1], Indent),
	go('Sentence', 'verification of reference count', [handle=Handle_sent2, ref_count=0], Indent),
	set_prolog_flag(exception_raised, false),
	catch(
	      delete_sentence(Handle_sent1),
	      lgp_api_error(sentence, still_referenced),
	      set_prolog_flag(exception_raised, true)
	     ),
	current_prolog_flag(exception_raised, true),
	delete_sentence(Handle_sent2), % Sentence 2 can be deleted though, because no linkage set is using it anymore
	go('Dictionary', 'verification of reference count', [handle=Handle_dict, ref_count=1], Indent),
	delete_all_sentences,
	go('Sentence', 'verification of reference count', [handle=Handle_sent1, ref_count=1], Indent).


execute_test_name('Dictionary', 'context creation of one object', Parms, Indent):-
	(   member(create_parms_dict=Create_parm_dict, Parms)
	->  true
	;   throw(test_lib_error('No create_parms_dict= parameter when calling context creation of a Dictionary'))
	),
	(   member(handle('Dictionary')=Handle_dict, Parms)
	->  true
	;   throw(test_lib_error('No handle(''Dictionary'')= parameter when calling context creation of a Dictionary'))
	),
	go('Dictionary', 'creation of one object', [create_parms=Create_parm_dict, handle=Handle_dict], Indent).

execute_test_name('Dictionary', 'context deletion of one object', Parms, Indent):-
	(   member(handle('Dictionary')=Handle_dict, Parms)
	->  true
	;   throw(test_lib_error('No handle(''Dictionary'')= parameter when calling context deletion of a Dictionary'))
	),
	go('Dictionary', 'deletion of one object', [handle=Handle_dict], Indent).

execute_test_name('Dictionary', 'addition of a reference', Parms, Indent):-
	!,
	member(create_parms_dict=Create_parm_dict, Parms),
	member(create_parms_sent=Create_parm_sent, Parms),
	go('Sentence', 'context creation of one object', [create_parms_dict=Create_parm_dict, create_parms_sent=Create_parm_sent, handle('Dictionary')=Handle_dict, handle('Sentence')=Handle_sent1], Indent),
	go('Dictionary', 'verification of reference count', [handle=Handle_dict, ref_count=1], Indent),
	go('Sentence', 'creation of one object', [create_parms=[Create_parm_sent, Handle_dict], handle=Handle_sent2], Indent),
	go('Dictionary', 'verification of reference count', [handle=Handle_dict, ref_count=2], Indent),
	go('Sentence', 'deletion of one object', [handle=Handle_sent1], Indent),
	go('Dictionary', 'verification of reference count', [handle=Handle_dict, ref_count=1], Indent),
	go('Sentence', 'deletion of one object', [handle=Handle_sent2], Indent),
	go('Dictionary', 'verification of reference count', [handle=Handle_dict, ref_count=0], Indent),
	go('Dictionary', 'context deletion of one object', [handle('Dictionary')=Handle_dict], Indent).

execute_test_name('Dictionary', 'deletion of non-existing handle', Parms, Indent):-
	set_prolog_flag(exception_raised, false),
	lgp_lib: get_handles_dictionaries(List_H),
	\+ member('$dictionary'(9999), List_H),
	catch(
	      go('Dictionary', 'deletion of one object', [handle='$dictionary'(-1)], Indent),
	      lgp_api_error(dictionary, cant_delete),
	      set_prolog_flag(exception_raised, true)
	     ),
	current_prolog_flag(exception_raised, true),
	set_prolog_flag(exception_raised, false),
	catch(
	      go('Dictionary', 'deletion of one object', [handle='$dictionary'(9999)], Indent),
	      lgp_api_error(dictionary, empty_index),
	      set_prolog_flag(exception_raised, true)
	     ),
	current_prolog_flag(exception_raised, true),
	go('Dictionary', 'context creation of one object', Parms, Indent),
	set_prolog_flag(exception_raised, false),
	catch(
	      go('Dictionary', 'deletion of one object', [handle=invalid(0)], Indent),
	      lgp_api_error(dictionary, bad_handle),
	      set_prolog_flag(exception_raised, true)
	     ),
	current_prolog_flag(exception_raised, true),
	go('Dictionary', 'context deletion of one object', Parms, Indent).
	

execute_test_name(Type_of_item, 'addition of a reference', Parms, Indent):-
	!,
	(   Type_of_item = 'Sentence' ; Type_of_item = 'Parse Options'), % This test checks both reference counts for both parse options and sentences, given that it needs to create a linkage set
	member(create_parms_dict=Create_parm_dict, Parms),
	member(create_parms_sent=Create_parm_sent, Parms),
	member(create_parms_opts=Create_parm_opts, Parms),
	go('Linkage Set', 'context creation of one object', [create_parms_dict=Create_parm_dict, create_parms_sent=Create_parm_sent, create_parms_opts=Create_parm_opts, handle('Dictionary')=Handle_dict, handle('Sentence')=Handle_sent, handle('Parse Options')=Handle_opts, handle('Linkage Set')=Handle_link1], Indent),
	go('Linkage Set', 'creation of one object', [create_parms=[Handle_sent, Handle_opts], handle=Handle_link2], Indent),
	go('Sentence', 'verification of reference count', [handle=Handle_sent, ref_count=2], Indent),
	go('Parse Options', 'verification of reference count', [handle=Handle_opts, ref_count=2], Indent),
	go('Linkage Set', 'deletion of one object', [handle=Handle_link1], Indent),
	go('Sentence', 'verification of reference count', [handle=Handle_sent, ref_count=1], Indent),
	go('Parse Options', 'verification of reference count', [handle=Handle_opts, ref_count=1], Indent),
	go('Linkage Set', 'deletion of one object', [handle=Handle_link2], Indent),
	go('Sentence', 'verification of reference count', [handle=Handle_sent, ref_count=0], Indent),
	go('Parse Options', 'verification of reference count', [handle=Handle_opts, ref_count=0], Indent),
	go('Sentence', 'context deletion of one object', [handle('Sentence')=Handle_sent, handle('Dictionary')=Handle_dict], Indent),
	go('Parse Options', 'context deletion of one object', [handle('Parse Options')=Handle_opts], Indent).

execute_test_name('Parse Options', 'context creation of one object', Parms, Indent):-
	(   member(create_parms_opts=Create_parm_opts, Parms)
	->  true
	;   throw(test_lib_rrror('No create_parms_opts= parameter when calling context creation of Parse Options'))
	),
	(   member(handle('Parse Options')=Handle_opts, Parms)
	->  true
	;   throw(test_lib_error('No handle(''Parse Options'') parameter when calling context creation of Parse Options'))
	),
	go('Parse Options', 'creation of one object', [create_parms=[Create_parm_opts], handle=Handle_opts], Indent).

execute_test_name('Parse Options', 'context deletion of one object', Parms, Indent):-
	(   member(handle('Parse Options')=Handle_opts, Parms)
	->  true
	;   throw(test_lib_error('No handle(''Parse Options'')= parameter when calling context deletion of Parse Options'))
	),
	go('Parse Options', 'deletion of one object', [handle=Handle_opts], Indent).

execute_test_name('Parse Options', 'deletion of non-existing handle', Parms, Indent):-
	set_prolog_flag(exception_raised, false),
	lgp_lib: get_handles_parse_options(List_H),
	\+ member('$options'(9999), List_H),
	catch(
	      go('Parse Options', 'deletion of one object', [handle='$options'(-1)], Indent),
	      lgp_api_error(parse_options, cant_delete),
	      set_prolog_flag(exception_raised, true)
	     ),
	current_prolog_flag(exception_raised, true),
	set_prolog_flag(exception_raised, false),
	catch(
	      go('Parse Options', 'deletion of one object', [handle='$options'(9999)], Indent),
	      lgp_api_error(parse_options, empty_index),
	      set_prolog_flag(exception_raised, true)
	     ),
	current_prolog_flag(exception_raised, true),
	go('Parse Options', 'context creation of one object', Parms, Indent),
	set_prolog_flag(exception_raised, false),
	catch(
	      go('Parse Options', 'deletion of one object', [handle=invalid(0)], Indent),
	      lgp_api_error(parse_options, bad_handle),
	      set_prolog_flag(exception_raised, true)
	     ),
	current_prolog_flag(exception_raised, true),
	go('Parse Options', 'context deletion of one object', Parms, Indent).


execute_test_name('Sentence', 'context creation of one object', Parms, Indent):-
	(   member(create_parms_sent=Create_parm_sent, Parms)
	->  true
	;   throw(test_lib_error('No create_parms_sent= parameter when calling context creation of a Sentence'))
	),
	(   member(handle('Sentence')=Handle_sent, Parms)
	->  true
	;   throw(test_lib_error('No handle(''Sentence'')= parameter when calling context creation of a Sentence'))
	),
	go('Dictionary', 'context creation of one object', Parms, Indent),
	member(handle('Dictionary')=Handle_dict, Parms),
	go('Sentence', 'creation of one object', [create_parms=[Create_parm_sent, Handle_dict], handle=Handle_sent], Indent).

execute_test_name('Sentence', 'context deletion of one object', Parms, Indent):-
	(   member(handle('Sentence')=Handle_sent, Parms)
	->  true
	;   throw(test_lib_error('No handle(''Sentence'')= parameter when calling context deletion of a Sentence'))
	),
	go('Sentence', 'deletion of one object', [handle=Handle_sent], Indent),
	go('Dictionary', 'context deletion of one object', Parms, Indent).


execute_test_name(Type_of_item, 'handles, references and count check', _Parms, _Indent):-
	!,
%	object_list_type_singular(Type_of_item, Object_singular),
	object_list_type_plural(Type_of_item, Object_plural),
	atom_concat(get_nb_, Object_plural, Get_nb_items),
	apply(lgp_lib: Get_nb_items, [Nb_items]),
	atom_concat(get_handles_, Object_plural, Get_handles_items),
	apply(lgp_lib: Get_handles_items, [Object_handle_list]),
	atom_concat(get_handles_nb_references_, Object_plural, Get_handles_nb_references_items),
	apply(lgp_lib: Get_handles_nb_references_items, [List_handles_with_references, List_references_with_handles]),
	(   length(Object_handle_list, Nb_items)
	->  true
	;   sformat(Exc_text, '~w gives a counts of ~w which does not match the lenght of handles in ~w returned by ~w~n', [Get_handles_items, Nb_items, Object_handle_list, Get_handles_items]),
	    throw(test_fail(Exc_text))
	),
	(   length(List_handles_with_references, Nb_items)
	->  true
	;   sformat(Exc_text, '~w gives a counts of ~w which does not match the lenght of handles in ~w returned by ~w~n', [Get_handles_items, Nb_items, List_handles_with_references, Get_handles_nb_references_items]),
	    throw(test_fail(Exc_text))
	),
	(   forall(member(One_handle, Object_handle_list), member(One_handle, List_handles_with_references))
	->  true
	;   sformat(Exc_text, '~w and ~w give different handle lists~n', [Get_handles_items, Get_handles_nb_references_items]),
	    throw(test_fail(Exc_text))
	),
	(   forall(member(One_nb_references, List_references_with_handles), One_nb_references >= 0)
	->  true
	;   sformat(Exc_text, '~w gives nb references <0~n', [Get_handles_nb_references_items]),
	    throw(test_fail(Exc_text))
	),
	(   length(List_references_with_handles, Nb_items)
	->  true
	;   sformat(Exc_text, '~w gives a list of nb_references ~w that does not contain the same amount of items as the handle list ~w~n', [Get_handles_nb_references_items, List_references_with_handles, List_handles_with_references]),
	    throw(test_fail(Exc_text))
	),
	true.

execute_test_name('Sentence', 'deletion of non-existing handle', Parms, Indent):-
	set_prolog_flag(exception_raised, false),
	lgp_lib: get_handles_sentences(List_H),
	\+ member('$sentence'(9999), List_H),
	catch(
	      go('Sentence', 'deletion of one object', [handle='$sentence'(-1)], Indent),
	      lgp_api_error(sentence, cant_delete),
	      set_prolog_flag(exception_raised, true)
	     ),
	current_prolog_flag(exception_raised, true),
	set_prolog_flag(exception_raised, false),
	catch(
	      go('Sentence', 'deletion of one object', [handle='$sentence'(9999)], Indent),
	      lgp_api_error(sentence, empty_index),
	      set_prolog_flag(exception_raised, true)
	     ),
	current_prolog_flag(exception_raised, true),
	go('Sentence', 'context creation of one object', Parms, Indent),
	set_prolog_flag(exception_raised, false),
	catch(
	      go('Sentence', 'deletion of one object', [handle=invalid(0)], Indent),
	      lgp_api_error(sentence, bad_handle),
	      set_prolog_flag(exception_raised, true)
	     ),
	current_prolog_flag(exception_raised, true),
	go('Sentence', 'context deletion of one object', Parms, Indent).


execute_test_name('Linkage Set', 'context creation of one object', Parms, Indent):-
	(   member(handle('Linkage Set')=Handle_link, Parms)
	->  true
	;   throw(test_lib_error('No handle(''Linkage Set'')= parameter when calling context deletion of a Linkage Set'))
	),
	go('Parse Options', 'context creation of one object', Parms, Indent),
	member(handle('Parse Options')=Handle_opts, Parms),
	go('Sentence', 'context creation of one object', Parms, Indent),
	member(handle('Sentence')=Handle_sent, Parms),
	go('Linkage Set', 'creation of one object', [create_parms=[Handle_sent, Handle_opts], handle=Handle_link], Indent).

execute_test_name('Linkage Set', 'context deletion of one object', Parms, Indent):-
	(   member(handle('Linkage Set')=Handle_link, Parms)
	->  true
	;   throw(test_lib_error('No handle(''Linkage Set'')= parameter when calling context deletion of a Linkage Set'))
	),
	go('Linkage Set', 'deletion of one object', [handle=Handle_link], Indent),
	go('Sentence', 'context deletion of one object', Parms, Indent),
	go('Parse Options', 'context deletion of one object', Parms, Indent).

execute_test_name('Linkage Set', 'deletion of non-existing handle', Parms, Indent):-
	set_prolog_flag(exception_raised, false),
	lgp_lib: get_handles_linkage_sets(List_H),
	\+ member('$linkage'(9999), List_H),
	catch(
	      go('Linkage Set', 'deletion of one object', [handle='$linkageset'(-1)], Indent),
	      lgp_api_error(linkage_set, cant_delete),
	      set_prolog_flag(exception_raised, true)
	     ),
	current_prolog_flag(exception_raised, true),
	set_prolog_flag(exception_raised, false),
	catch(
	      go('Linkage Set', 'deletion of one object', [handle='$linkageset'(9999)], Indent),
	      lgp_api_error(linkage_set, empty_index),
	      set_prolog_flag(exception_raised, true)
	     ),
	current_prolog_flag(exception_raised, true),
	go('Linkage Set', 'context creation of one object', Parms, Indent),
	set_prolog_flag(exception_raised, false),
	catch(
	      go('Linkage Set', 'deletion of one object', [handle=invalid(0)], Indent),
	      lgp_api_error(linkage_set, bad_handle),
	      set_prolog_flag(exception_raised, true)
	     ),
	current_prolog_flag(exception_raised, true),
	go('Linkage Set', 'context deletion of one object', Parms, Indent).

execute_test_name('Linkage Set', 'deletion of linkage set object before cut of get_linkage', Parms, Indent):-
	!,
	go('Linkage Set', 'context creation of one object', Parms, Indent),
	member(handle('Linkage Set')=Handle_link, Parms),
	(
	    set_prolog_flag(exception_raised, false),
	    repeat,
	    catch(
		  get_linkage(Handle_link, _Linkage), % The linkage set being a multiple solution one, it is now still possible to redo the predicate. A context is thus still waiting
		  lgp_api_error(linkage_set, reference_erased),
		  set_prolog_flag(exception_raised, true)
		 ),
	    get_handles_linkage_sets(List_handles_linkage_sets),
	    (	member(Handle_link, List_handles_linkage_sets) % If the handle of the linkage set we use to extract linkages is still in memory
	    ->	delete_linkage_set(Handle_link),
		fail
	    ;	current_prolog_flag(exception_raised, true) % Otherwise, make sure that an exception has been raised
	    )
%	    fail  % Fail to force redo of the get_linkage/2 call above
	),
	go('Sentence', 'context deletion of one object', Parms, Indent),
	go('Parse Options', 'context deletion of one object', Parms, Indent).


execute_test_name(Type_of_item, 'creation/deletion', Parms, Indent):-
	!,
	(   Type_of_item='Dictionary'
	->  findall(One_parm,
		    (	member(One_parm, Parms), One_parm \= (handle('Dictionary')=_)),
		    New_parms_wo_handle), % This removes any parameter following the template handle('Dictionary')=_ from the parameters in Parms and stores the result in New_parms_wo_handle
	    New_parms_w_handle=[handle('Dictionary')=_Handle_dict|New_parms_wo_handle] % This specifically creates a free variable as the handle('Dictionary')=_ parameter in Parms and saves it in New_parms_w_handle
	;   Type_of_item='Sentence'
	->  findall(One_parm,
		    (	member(One_parm, Parms), One_parm \= (handle('Sentence')=_)),
		    New_parms_wo_handle), % This removes any parameter following the template handle('Sentence')=_ from the parameters in Parms and stores the result in New_parms_wo_handle
	    New_parms_w_handle=[handle('Sentence')=_Handle_sent|New_parms_wo_handle] % This specifically creates a free variable as the handle('Sentence')=_ parameter in Parms and saves it in New_parms_w_handle
	;   Type_of_item='Parse Options'
	->  findall(One_parm,
		    (	member(One_parm, Parms), One_parm \= (handle('Parse Options')=_)),
		    New_parms_wo_handle), % This removes any parameter following the template handle('Parse Options')=_ from the parameters in Parms and stores the result in New_parms_wo_handle
	    New_parms_w_handle=[handle('Parse Options')=_Handle_dict|New_parms_wo_handle] % This specifically creates a free variable as the handle('Parse Options')=_ parameter in Parms and saves it in New_parms_w_handle
	;   Type_of_item='Linkage Set'
	->  findall(One_parm,
		    (	member(One_parm, Parms), One_parm \= (handle('Linkage Set')=_)),
		    New_parms_wo_handle), % This removes any parameter following the template handle('Linkage Set')=_ from the parameters in Parms and stores the result in New_parms_wo_handle
	    New_parms_w_handle=[handle('Linkage Set')=_Handle_dict|New_parms_wo_handle] % This specifically creates a free variable as the handle('Linkage Set')=_ parameter in Parms and saves it in New_parms_w_handle
	),
	% All the previous group of cases made sure that the handle_???? parameter in Parms has been removed and readded as a free variable, which allows us to pass it to both the context creation and context deletion predicates, which will pass their handle via this free variable. If the above cases were not checked, an already bound handle_????, or a missing one in the list Parms would make the deletion fail directly, since it would not have any idea about the handle to delete
	go(Type_of_item, 'context creation of one object', New_parms_w_handle, Indent),
	go(Type_of_item, 'context deletion of one object', New_parms_w_handle, Indent).

% This requires an option handle=Handle for the item searched
% It also needs the reference count associated with this handle as ref_count=Ref_count
execute_test_name(Type_of_item, 'verification of reference count', Parms, _Indent):-
	!,
%	object_list_type_singular(Type_of_item, Object_singular),
	object_list_type_plural(Type_of_item, Object_plural),
	atom_concat(get_handles_nb_references_, Object_plural, Get_handles_nb_references_items),
	apply(lgp_lib: Get_handles_nb_references_items, [List_handles_with_references, List_references_with_handles]),
	member(handle=Handle, Parms),
	member(ref_count=Count_expected, Parms),
	nth1(Position_handle, List_handles_with_references, Handle),
	nth1(Position_handle, List_references_with_handles, Actual_count),
	(   Actual_count = Count_expected
	->  true
	;   sformat(Exc_text, 'Count reference for handle ~w is ~w and does not match with expected value ~w~n', [Handle, Actual_count, Count_expected]),
	    throw(test_fail(Exc_text))
	).
	
% Note: the following tests needs to know if create on the item has additionnal parameters (these parameters will be set as create_parms=[Arg1, Arg2|...]. The last parameter will be added automatically as the handle returned by create_*
% An optional handle=Handle parameter allows to get back the handle of the object created
execute_test_name(Type_of_item, 'creation of one object', Parms, Indent):-
	!,
	object_list_type_singular(Type_of_item, Object_singular),
	object_list_type_plural(Type_of_item, Object_plural),
	atom_concat(get_nb_, Object_plural, Get_nb_items),
	apply(lgp_lib: Get_nb_items, [Initial_nb_items]),
	go(Type_of_item, 'handles, references and count check', [], Indent),
	atom_concat(get_handles_nb_references_, Object_plural, Get_handles_nb_references_items),
	apply(lgp_lib: Get_handles_nb_references_items,
	      [List_initial_handles_with_references, _List_initial_references_with_handles]),
	(   member(create_parms=List_of_parms, Parms)
	->  true
	;   List_of_parms=[]
	),
	atom_concat(create_, Object_singular, Create_item),
	append(List_of_parms, [Handle], List_of_parms_for_create),
	apply(lgp_lib: Create_item, List_of_parms_for_create),
	(   member(handle=Parm_return_handle, Parms) % If a handle to the newly created object is asked in the parameters...
	->  Handle = Parm_return_handle % Unify this parameter with the new handle created
	;   true
	),
	apply(lgp_lib: Get_nb_items, [Final_nb_items]),
	Planned_final_nb_items is Initial_nb_items + 1,
	(   Planned_final_nb_items = Final_nb_items
	->  true
	;   sformat(Exc_text, 'Nb of ~w handles was ~w before creation and ~w after~n',
		   [Type_of_item, Initial_nb_items, Final_nb_items]),
	    throw(test_fail(Exc_text))
	),
	go(Type_of_item, 'handles, references and count check', [], Indent),
	apply(lgp_lib: Get_handles_nb_references_items,
	      [List_final_handles_with_references, _List_final_references_with_handles]),
	(   member(Handle, List_final_handles_with_references)
	->  true
	;   sformat(Exc_text, 'Handle of newly created ~w not found in handle list ~w~n',
		   [Handle, List_final_handles_with_references]),
	    throw(test_fail(Exc_text))
	),
	go(Type_of_item, 'verification of reference count', [handle=Handle, ref_count=0], Indent),
	(   forall(member(One_handle, List_initial_handles_with_references),
		   member(One_handle, List_final_handles_with_references)
		  )
	->  true
	;   sformat(Exc_text, 'Handles present before creation are not present after creation of new ~w~n',
		   [Type_of_item]),
	    throw(test_fail(Exc_text))
	),
	true.

% Note: the following test needs to know which handle to delete (these parameters will be set as handle=Handle.
execute_test_name(Type_of_item, 'deletion of one object', Parms, Indent):-
	!,
	object_list_type_singular(Type_of_item, Object_singular),
	object_list_type_plural(Type_of_item, Object_plural),
	atom_concat(get_nb_, Object_plural, Get_nb_items),
	apply(lgp_lib: Get_nb_items, [Initial_nb_items]),
	go(Type_of_item, 'handles, references and count check', [], Indent),
	atom_concat(get_handles_nb_references_, Object_plural, Get_handles_nb_references_items),
	apply(lgp_lib: Get_handles_nb_references_items, [List_initial_handles_with_references, _List_initial_references_with_handles]),
	(   member(handle=Handle, Parms)
	->  true
	;   sformat(Exc_text, 'Parms need to contain a handle_item when calling test "deletion of one object" (Parms=~w)~n', [Parms]),
	    throw(test_fail(Exc_Text))
	),
	atom_concat(delete_, Object_singular, Delete_item),
	apply(lgp_lib: Delete_item, [Handle]),
	apply(lgp_lib: Get_nb_items, [Final_nb_items]),
	Planned_final_nb_items is Initial_nb_items - 1,
	(   Planned_final_nb_items = Final_nb_items
	->  true
	;   sformat(Exc_text, 'Nb of ~w handles was ~w before deletion and ~w after~n', [Type_of_item, Initial_nb_items, Final_nb_items]),
	    throw(test_fail(Exc_Text))
	),
	go(Type_of_item, 'handles, references and count check', [], Indent),
	apply(lgp_lib: Get_handles_nb_references_items, [List_final_handles_with_references, _List_final_references_with_handles]),
	(   \+ member(Handle, List_final_handles_with_references)
	->  true
	;   sformat(Exc_text, 'Handle of deleted ~w is still in handle list ~w~n', [Handle, List_final_handles_with_references]),
 	    throw(test_fail(Exc_Text))
	),
	(   forall((member(One_handle, List_initial_handles_with_references), One_handle \=Handle), member(One_handle, List_final_handles_with_references))
	->  true
	;   sformat(Exc_text, 'Handles present before deletion have been deleted while deleting ~w~n', [Type_of_item]),
 	    throw(test_fail(Exc_Text))
	),
	true.

execute_test_name(Type_of_item, 'deletion of the whole list', _Parms, Indent):-
	!,
%	object_list_type_singular(Type_of_item, Object_singular),
	object_list_type_plural(Type_of_item, Object_plural),
	atom_concat(delete_all_, Object_plural, Delete_all_items),
	lgp_lib: Delete_all_items,
	atom_concat(get_nb_, Object_plural, Get_nb_items),
	apply(lgp_lib: Get_nb_items, [Nb_items_after_deletion]),
	(   Nb_items_after_deletion = 0
	->  true
	;   sformat(Exc_text, 'Nb items not set to zero (~w) after ~w~n', [Nb_items_after_deletion, Delete_all_items]),
 	    throw(test_fail(Exc_Text))	    
	),
	atom_concat(get_handles_, Object_plural, Get_handles_items),
	apply(lgp_lib: Get_handles_items, [List_handles_after_deletion]),
	(   List_handles_after_deletion = []
	->  true
	;   sformat(Exc_text, 'List of handles is not empty (~w) after ~w~n', [List_handles_after_deletion, Delete_all_items]),
 	    throw(test_fail(Exc_Text))
	),
	atom_concat(get_handles_nb_references_, Object_plural, Get_handles_nb_references_items),
	apply(lgp_lib: Get_handles_nb_references_items, [List_final_handles_with_references_after_deletion, List_final_references_with_handles_after_deletion]),
	(   List_final_handles_with_references_after_deletion = []
	->  true
	;   sformat(Exc_text, 'List of handles is not empty (~w) after ~w~n', [List_final_handles_with_references_after_deletion , Delete_all_items]),
 	    throw(test_fail(Exc_Text))	    
	),
	(   List_final_references_with_handles_after_deletion = []
	->  true
	;   sformat(Exc_text, 'List of reference count is not empty (~w) after ~w~n', [List_final_references_with_handles_after_deletion , Delete_all_items]),
 	    throw(test_fail(Exc_Text))	    
	),
	go(Type_of_item, 'handles, references and count check', [], Indent),
	true.

execute_test_name('Dictionary', 'multiple creation/deletion'):-
	!,
	lgp_lib:delete_all_dictionaries,
	lgp_lib:create_dictionary('4.0.dict', '4.0.knowledge', '4.0.constituent-knowledge', '4.0.affix', Dictionary_handle_1),
	lgp_lib:create_dictionary('4.0.dict', '4.0.knowledge', '4.0.constituent-knowledge', '4.0.affix', Dictionary_handle_2),
	lgp_lib:create_dictionary('4.0.dict', '4.0.knowledge', '4.0.constituent-knowledge', '4.0.affix', Dictionary_handle_3),
	lgp_lib:create_dictionary('4.0.dict', '4.0.knowledge', '4.0.constituent-knowledge', '4.0.affix', Dictionary_handle_4),
	lgp_lib:create_dictionary('4.0.dict', '4.0.knowledge', '4.0.constituent-knowledge', '4.0.affix', Dictionary_handle_5),
	lgp_lib:get_nb_dictionaries(5),
	lgp_lib:get_handles_dictionaries(LDH1),
	memberchk(Dictionary_handle_1, LDH1),
	memberchk(Dictionary_handle_2, LDH1),
	memberchk(Dictionary_handle_3, LDH1),
	memberchk(Dictionary_handle_4, LDH1),
	memberchk(Dictionary_handle_5, LDH1),
	lgp_lib:get_handles_nb_references_dictionaries(LDH2, LNBRef2),
	memberchk(Dictionary_handle_1, LDH2),
	memberchk(Dictionary_handle_2, LDH2),
	memberchk(Dictionary_handle_3, LDH2),
	memberchk(Dictionary_handle_4, LDH2),
	memberchk(Dictionary_handle_5, LDH2),
	\+ (member(NBRef2, LNBRef2), NBRef2 \= 0), % Check that all reference counts equal 0
	true.

	