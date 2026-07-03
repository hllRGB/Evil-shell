// vim: set ts=8 sts=8 sw=8 et:

%token IF THEN ELSE ELIF FI CASE ESAC FOR SELECT WHILE UNTIL DO DONE FUNCTION COPROC
//     if then else elif fi case esac for select while until do done function coproc

%token COND_START COND_END COND_ERROR
//     [[        ]]        (error token)

%token IN BANG TIME TIMEOPT TIMEIGN
//     in !    time -p      --

%token <word> WORD ASSIGNMENT_WORD REDIR_WORD  //值是word
//            xwx  FOO=bar    {varname}

%token <number> NUMBER     //值是number
//             (number literal)

%token <word_list> ARITH_CMD ARITH_FOR_EXPRS   //值是word_list
//               ((...))    ((xx;xx;xx))

%token <command> COND_CMD   //值是commmand
//               (test expr)

%token AND_AND OR_OR GREATER_GREATER LESS_LESS LESS_AND LESS_LESS_LESS
//     &&      ||    >>              <<        <&       <<<

%token GREATER_AND SEMI_SEMI SEMI_AND SEMI_SEMI_AND
//     >&          ;;        ;&       ;;&

%token LESS_LESS_MINUS AND_GREATER AND_GREATER_GREATER LESS_GREATER
//     <<-             &>          &>>                 <>

%token GREATER_BAR BAR_AND
//     >|          |&

// %token DOLPAREN
// //     $(
// 
// %token DOLBRACE
// //     ${

%type <command> inputunit command pipeline pipeline_command
%type <command> list0 list1 compound_list simple_list simple_list1
%type <command> simple_command shell_command
%type <command> for_command select_command case_command group_command
%type <command> arith_command
%type <command> cond_command
%type <command> arith_for_command
%type <command> coproc
//%type <command> comsub funsub
%type <command> function_def function_body if_command elif_clause subshell
%type <redirect> redirection redirection_list
%type <element> simple_command_element
%type <word_list> word_list pattern
%type <pattern> pattern_list case_clause_sequence case_clause
%type <number> timespec
%type <number> list_terminator simple_list_terminator
%type <number> nullcmd_terminator



%start inputunit






%left '&' ';' '\n' yacc_EOF
%left AND_AND OR_OR
%right '|' BAR_AND
%%

inputunit:	simple_list simple_list_terminator	// echo hi\n
//         |	comsub					// $(echo hi)
//                |	funsub				// ${ echo hi; }
//                |	'\n'				// <empty line>
//                |	error '\n'			// syntax error recovery
//                |	error yacc_EOF			// EOF after error
//                |	error YYEOF			// EOF after error (bison)
//                |	yacc_EOF			// <EOF>
                ;




//WORDs.
word_list:	WORD					// foo
         |	word_list WORD				// foo bar baz
                ;

redirection:	'>' WORD					// >file
           |	'<' WORD					// <file
                |	NUMBER '>' WORD				// 2>file
                |	NUMBER '<' WORD				// 0<file
                |	REDIR_WORD '>' WORD			// {varname}>file
                |	REDIR_WORD '<' WORD			// {varname}<file
                |	GREATER_GREATER WORD			// >>file
                |	NUMBER GREATER_GREATER WORD		// 2>>file
                |	REDIR_WORD GREATER_GREATER WORD		// {varname}>>file
                |	GREATER_BAR WORD			// >|file
                |	NUMBER GREATER_BAR WORD			// 2>|file
                |	REDIR_WORD GREATER_BAR WORD		// {varname}>|file
                |	LESS_GREATER WORD			// <>file
                |	NUMBER LESS_GREATER WORD		// 0<>file
                |	REDIR_WORD LESS_GREATER WORD		// {varname}<>file
                |	LESS_LESS WORD				// <<EOF
                |	NUMBER LESS_LESS WORD			// 1<<EOF
                |	REDIR_WORD LESS_LESS WORD		// {varname}<<EOF
                |	LESS_LESS_MINUS WORD			// <<-EOF
                |	NUMBER LESS_LESS_MINUS WORD		// 1<<-EOF
                |	REDIR_WORD  LESS_LESS_MINUS WORD	// {varname}<<-EOF
                |	LESS_LESS_LESS WORD			// <<<word
                |	NUMBER LESS_LESS_LESS WORD		// 0<<<word
                |	REDIR_WORD LESS_LESS_LESS WORD		// {varname}<<<word
                |	LESS_AND NUMBER				// <&1
                |	NUMBER LESS_AND NUMBER			// 0<&1
                |	REDIR_WORD LESS_AND NUMBER		// {varname}<&1
                |	GREATER_AND NUMBER			// >&2
                |	NUMBER GREATER_AND NUMBER		// 1>&2
                |	REDIR_WORD GREATER_AND NUMBER		// {varname}>&2
                |	LESS_AND WORD				// <&$fd
                |	NUMBER LESS_AND WORD			// 0<&$fd
                |	REDIR_WORD LESS_AND WORD		// {varname}<&$fd
                |	GREATER_AND WORD			// >&$fd
                |	NUMBER GREATER_AND WORD			// 1>&$fd
                |	REDIR_WORD GREATER_AND WORD		// {varname}>&$fd
                |	GREATER_AND '-'				// >&-
                |	NUMBER GREATER_AND '-'			// 1>&-
                |	REDIR_WORD GREATER_AND '-'		// {varname}>&-
                |	LESS_AND '-'				// <&-
                |	NUMBER LESS_AND '-'			// 0<&-
                |	REDIR_WORD LESS_AND '-'			// {varname}<&-
                |	AND_GREATER WORD			// &>file
                |	AND_GREATER_GREATER WORD		// &>>file
                ;

simple_command_element: WORD				// ls
                      |	ASSIGNMENT_WORD			// FOO=bar
                      |	redirection			// >file
                ;
//我们可能需要WORD SWITCH。

//多个redir.
redirection_list: redirection				// >file
                |	redirection_list redirection	// >file 2>&1
                ;

//多个ele.
simple_command:	simple_command_element			// ls
              |	simple_command simple_command_element	// ls -la
                ;



//简单命令和复合命令。
command:	simple_command				// ls -la
       |	shell_command				// for i in x; do echo $i; done
                |	shell_command redirection_list	// { echo hi; } >file
                |	function_def			// f() { echo hi; }
                |	coproc				// coproc P { sleep 1; }
                ;


//纯复合命令
shell_command:	for_command						// for i in x; do echo $i; done
             |	case_command						// case $x in a) echo;; esac
        |	WHILE compound_list DO compound_list DONE	// while true; do echo hi; done
                |	UNTIL compound_list DO compound_list DONE	// until false; do echo hi; done
                |	select_command					// select x in a b; do echo $x; done
                |	if_command					// if true; then echo ok; fi
                |	subshell					// (echo hi)
                |	group_command					// { echo hi; }
                |	arith_command					// (( x + 1 ))
                |	cond_command					// [[ -f file ]]
                |	arith_for_command				// for ((i=0;i<10;i++)); do echo $i; done
                ;


for_command:	FOR WORD newline_list DO compound_list DONE							// for i in x; do echo $i; done
           |	FOR WORD newline_list '{' compound_list '}'							// for i in x; { echo $i; }
                |	FOR WORD ';' newline_list DO compound_list DONE						// for i; do echo $i; done
                |	FOR WORD ';' newline_list '{' compound_list '}'						// for i; { echo $i; }
                |	FOR WORD newline_list IN word_list list_terminator newline_list DO compound_list DONE	// for i in a b; do echo $i; done
                |	FOR WORD newline_list IN word_list list_terminator newline_list '{' compound_list '}'	// for i in a b; { echo $i; }
                |	FOR WORD newline_list IN list_terminator newline_list DO compound_list DONE		// for i in "$@"; do echo $i; done
                |	FOR WORD newline_list IN list_terminator newline_list '{' compound_list '}'		// for i in "$@"; { echo $i; }
                ;

arith_for_command:	FOR ARITH_FOR_EXPRS list_terminator newline_list DO compound_list DONE			// for ((i=0;i<10;i++)); do echo $i; done
                 |		FOR ARITH_FOR_EXPRS list_terminator newline_list '{' compound_list '}'		// for ((i=0;i<10;i++)); { echo $i; }
                |		FOR ARITH_FOR_EXPRS DO compound_list DONE					// for ((;;)); do break; done
                |		FOR ARITH_FOR_EXPRS '{' compound_list '}'					// for ((;;)); { break; }
                ;

//SELECT switch.
select_command:	SELECT WORD newline_list DO compound_list DONE								// select i in x; do echo $i; done
              |	SELECT WORD newline_list '{' compound_list '}'								// select i in x; { echo $i; }
                |	SELECT WORD ';' newline_list DO compound_list DONE						// select i; do echo $i; done
                |	SELECT WORD ';' newline_list '{' compound_list '}'						// select i; { echo $i; }
                |	SELECT WORD newline_list IN word_list list_terminator newline_list DO compound_list DONE	// select i in a b; do echo $i; done
                |	SELECT WORD newline_list IN word_list list_terminator newline_list '{' compound_list '}'	// select i in a b; { echo $i; }
                |	SELECT WORD newline_list IN list_terminator newline_list DO compound_list DONE			// select i in "$@"; do echo $i; done
                |	SELECT WORD newline_list IN list_terminator newline_list '{' compound_list '}'			// select i in "$@"; { echo $i; }
                ;
//CASE switch.
case_command:	CASE WORD newline_list IN newline_list ESAC				// case $x in esac
            |	CASE WORD newline_list IN case_clause_sequence newline_list ESAC	// case $x in a) echo;; esac
                |	CASE WORD newline_list IN case_clause ESAC			// case $x in a) echo;; esac
                ;
//FUNCTIOM switch,WORD () switch.
function_def:	WORD '(' ')' newline_list function_body					// f() { echo hi; }
            |	FUNCTION WORD '(' ')' newline_list function_body			// function f() { echo hi; }
                |	FUNCTION WORD function_body					// function f { echo hi; }
                |	FUNCTION WORD '\n' newline_list function_body			// function fU\n{ echo hi; }
                ;
//必须是复合命令了.
function_body:	shell_command				// { echo hi; }
             |	shell_command redirection_list		// { echo hi; } >file
                ;
//( switch.
subshell:	'(' compound_list ')'	// (echo hi)
        ;

//comsub:		DOLPAREN compound_list ')'		// $(echo hi)
//      |	DOLPAREN newline_list ')'			// $(<newline>)
//                ;
//
//funsub:		DOLBRACE compound_list '}'		// ${ echo hi; }
//      |	DOLBRACE newline_list '}'			// ${<newline>}
//                ;

coproc:		COPROC shell_command			// coproc { sleep 1; }
      |	COPROC shell_command redirection_list		// coproc { sleep 1; } >file
                |	COPROC WORD shell_command			// coproc P { sleep 1; }
                |	COPROC WORD shell_command redirection_list	// coproc P { sleep 1; } >file
                |	COPROC simple_command				// coproc sleep 10
                ;

if_command:	IF compound_list THEN compound_list FI				// if true; then echo ok; fi
          |	IF compound_list THEN compound_list ELSE compound_list FI	// if true; then echo y; else echo n; fi
                |	IF compound_list THEN compound_list elif_clause FI	// if true; then echo ok; elif false; then echo n; fi
                ;

//命令组
group_command:	'{' compound_list '}'					// { echo hi; }
             ;

arith_command:	ARITH_CMD	//wtf					// (( x + 1 ))
             ;
//[[ xx ]]
cond_command:	COND_START COND_CMD COND_END				// [[ -f file ]]
            ;

elif_clause:	ELIF compound_list THEN compound_list				// elif true; then echo ok
           |	ELIF compound_list THEN compound_list ELSE compound_list	// elif true; then echo y; else echo n
                |	ELIF compound_list THEN compound_list elif_clause	// elif true; then echo ok; elif false; then echo n
                ;

case_clause:	pattern_list						// a) echo;;
           |	case_clause_sequence pattern_list			// a) echo;; b) echo;;
                ;

pattern_list:	newline_list pattern ')' compound_list			// a) echo;;
            |	newline_list pattern ')' newline_list			// a) <fallthrough>
                |	newline_list '(' pattern ')' compound_list	// (a) echo;;
                |	newline_list '(' pattern ')' newline_list	// (a) <fallthrough>
                ;
//几个元素的多次匹配。
case_clause_sequence:  pattern_list SEMI_SEMI				// a) echo;;
                    |	case_clause_sequence pattern_list SEMI_SEMI	// a) echo;; b) echo;;
                |	pattern_list SEMI_AND				// a) echo;& (fallthrough)
                |	case_clause_sequence pattern_list SEMI_AND	// a) echo;& b) echo;&
                |	pattern_list SEMI_SEMI_AND			// a) echo;;& (continue)
                |	case_clause_sequence pattern_list SEMI_SEMI_AND	// a) echo;;& b) echo;;&
                ;

//case用的   a|b)xx;;
pattern:	WORD						// a
       |	pattern '|' WORD				// a|b
                ;
//只加了个匹配换行符，看起来可以并。
// 本质就是pipeline加连接符。
compound_list:	newline_list list0				// \n cmd; cmd
             |	newline_list list1				// \n cmd && cmd
                ;

//一元终结 ,考虑并到二元。
list0:  	list1 '\n' newline_list			// cmd\n
     |	list1 '&' newline_list				// cmd&
                |	list1 ';' newline_list		// cmd;
                ;
//二元连接
list1:		list1 AND_AND newline_list list1	// cmd && cmd
                |	list1 OR_OR newline_list list1	// cmd || cmd
                |	list1 '&' newline_list list1	// cmd & cmd
                |	list1 ';' newline_list list1	// cmd ; cmd
                |	list1 '\n' newline_list list1	// cmd\ncmd
                |	pipeline_command		// cmd | cmd
                ;

simple_list_terminator:	'\n'				// \n
                      |	yacc_EOF			// EOF
                ;

list_terminator:'\n'					// \n
               |	';'				// ;
                |	yacc_EOF			// EOF
                ;

nullcmd_terminator:	list_terminator			// \n or ;
                  |	'&'				// &
                ;
//多个换行符
newline_list:						// empty
            |	newline_list '\n'		// \n\n
                ;

//只是用来加终结符
simple_list:	simple_list1			// cmd && cmd
           |	simple_list1 '&'		// cmd&
                |	simple_list1 ';'	// cmd;
                ;
//main
simple_list1:	simple_list1 AND_AND newline_list simple_list1	// cmd && cmd
            |	simple_list1 OR_OR newline_list simple_list1	// cmd || cmd
                |	simple_list1 '&' simple_list1		// cmd & cmd
                |	simple_list1 ';' simple_list1		// cmd ; cmd
                |	pipeline_command			// cmd | cmd
                ;
//匹配pipeline.
pipeline_command: pipeline					// cmd | cmd
                |	BANG pipeline_command			// ! cmd
                |	timespec pipeline_command		// time cmd
                |	timespec nullcmd_terminator		// time; time\n
                |	BANG nullcmd_terminator			// !\n (POSIX 267)
                ;

pipeline:	pipeline '|' newline_list pipeline	// cmd | cmd
        |	pipeline BAR_AND newline_list pipeline	// cmd |& cmd
                |	command				// ls -la
                ;

timespec:	TIME					// time
        |	TIME TIMEOPT				// time -p
                |	TIME TIMEIGN			// time -- (ignore)
                |	TIME TIMEOPT TIMEIGN		// time -p --
                ;
%%
