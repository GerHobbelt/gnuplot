/*
 *
 *    G N U P L O T  --  command.c
 *
 *  Copyright (C) 1986 Thomas Williams, Colin Kelley
 *
 *  You may use this code as you wish if credit is given and this message
 *  is retained.
 *
 *  Please e-mail any useful additions to vu-vlsi!plot so they may be
 *  included in later releases.
 *
 *  This file should be edited with 4-column tabs!  (:set ts=4 sw=4 in vi)
 */

#include <stdio.h>
#include <math.h>
#include "plot.h"

#ifndef STDOUT
#define STDOUT 1
#endif

/*
 * global variables to hold status of 'set' options
 *
 */
BOOLEAN                 autoscale       = TRUE;
enum PLOT_STYLE data_style      = POINTS,
                                func_style      = LINES;
BOOLEAN                 log_x           = FALSE,
                                log_y           = FALSE;
FILE*                   outfile;
char                    outstr[MAX_ID_LEN+1] = "STDOUT";
int                             samples         = SAMPLES;
int                             term            = 0;                            /* unknown term is 0 */
double                  xmin            = -10,
                                xmax            = 10,
                                ymin            = -10,
                                ymax            = 10;
double                  zero = ZERO;                    /* zero threshold, not 0! */


BOOLEAN screen_ok;
BOOLEAN term_init;
BOOLEAN undefined;

/*
 * instead of <strings.h>
 */

char *gets(),*getenv();
char *strcpy(),*strcat();

double magnitude(),angle(),real(),imag();
struct value *const_express(), *pop(), *complex();


extern struct vt_entry vt[];
extern struct udft_entry udft[];
extern struct termentry term_tbl[];

char input_line[MAX_LINE_LEN],dummy_var[MAX_ID_LEN + 1];
struct at_type *curr_at;
int c_token;
int next_value = (int)NEXT_VALUE,next_function = 0,c_function = 0;
int num_tokens;

struct curve_points plot[MAX_PLOTS];

static char help[80] = HELP;

#ifdef MSDOS
#include <process.h>
#endif /* MSDOS */

#ifdef vms
#include <descrip.h>
#include <rmsdef.h>
#include <errno.h>

extern lib$get_input(), lib$put_output();

int vms_len;

unsigned int status[2] = {1, 0};

$DESCRIPTOR(prompt_desc,PROMPT);
$DESCRIPTOR(line_desc,input_line);
$DESCRIPTOR(null_desc,"");

$DESCRIPTOR(help_desc,help);
$DESCRIPTOR(helpfile_desc,"GNUPLOT$HELP");

struct dsc$descriptor_s *cmd_ptr;

#endif

com_line()
{
#ifdef vms

        switch(status[1] = lib$get_input(&line_desc, &prompt_desc, &vms_len)){
                case RMS$_EOF:
                        done(IO_SUCCESS);       /* ^Z isn't really an error */
                        break;
                case RMS$_TNS:                  /* didn't press return in time */
                        vms_len--;              /* skip the last character */
                        break;                  /* and parse anyway */
                case RMS$_BES:                  /* Bad Escape Sequence */
                case RMS$_PES:                  /* Partial Escape Sequence */
                        sys$putmsg(status);
                        vms_len = 0;            /* ignore the line */
                        break;
                case SS$_NORMAL:
                        break;                  /* everything's fine */
                default:
                        done(status[1]);        /* give the error message */
        }

        if (vms_len && (input_line[0] == '!' || input_line[0] == '$')) {
                if (vms_len == 1)       /* spawn an interactive shell */
                        cmd_ptr = &null_desc;
                else {
                        cmd_ptr = &line_desc;
                        input_line[0] = ' ';    /* an embarrassment, but... */
                }

                if ((status[1] = lib$spawn(cmd_ptr)) != SS$_NORMAL) {
                        sys$putmsg(status);
                }
                (void) putchar('\n');

        } else {
                input_line[vms_len] = '\0';

#else
                /* not VMS */
        fprintf(stderr,PROMPT);
#ifdef MSDOS
        input_line[0] = MAX_LINE_LEN - 2;
        cgets(input_line);                      /* console input so CED will work */
        (void) putc('\n',stderr);
        if (input_line[2] == 26) {
                (void) putc('\n',stderr);               /* end-of-file */
                done(IO_SUCCESS);
        }
        {
                register int i = -1;
                do                              /* yuck!  move everything down two characters */
                        i++;
                while (input_line[i] = input_line[i+2]);
        }

#else /* MSDOS */
                /* plain old Unix */
        if (!(int) gets(input_line)) {
                (void) putc('\n',stderr);               /* end-of-file */
                done(IO_SUCCESS);
        }
#endif /* MSDOS */

        screen_ok = TRUE; /* so we can flag any new output */

        if (input_line[0] == '!') {
                if (system(input_line + 1))
                        os_error("system() failed",NO_CARET);
        } else {
#endif /* vms */
                do_line();
        }
}



do_line()  /* also used in load_file */
{
        num_tokens = scanner(input_line);
        c_token = 0;
        while(c_token < num_tokens) {
                command();
                if (c_token < num_tokens)       /* something after command */
                        if (equals(c_token,";"))
                                c_token++;
                        else
                                int_error("';' expected",c_token);
        }
}



command()
{
register int tsamp,len;
register FILE *f;
struct value a;
static char sv_file[MAX_ID_LEN+1];
                        /* string holding name of save or load file */

        dummy_var[0] = '\0';            /* no dummy variable */

        if (is_definition(c_token))
                define();
        else if (equals(c_token,"help") || equals(c_token,"?")) {
                c_token++;
                len = sizeof(HELP)-1;
                help[len] = '\0';               /* remove previous help arguments */
                while (!(END_OF_COMMAND)) {
                        help[len] = ' ';   /* put blank between help segments */
                        copy_str(help+len+1,c_token++);
                        len = strlen(help);
                }
#ifdef vms
                help_desc.dsc$w_length = len;
                if ((vaxc$errno = lbr$output_help(lib$put_output,0,&help_desc,
                        &helpfile_desc,0,lib$get_input)) != SS$_NORMAL)
                                os_error("can't open GNUPLOT$HELP");
#else /* vms */
                if (system(help))
                        os_error("can't spawn help");
#endif /* vms */
                screen_ok = FALSE;
                c_token++;
        }
        else if (almost_equals(c_token,"pr$int")) {
                c_token++;
                (void) const_express(&a);
                (void) putc('\t',stderr);
                show_value(stderr,&a);
                (void) putc('\n',stderr);
                screen_ok = FALSE;
        }
        else if (almost_equals(c_token,"p$lot")) {
                c_token++;
                plotrequest();
        }
        else if (almost_equals(c_token,"se$t")) {
                if (almost_equals(++c_token,"t$erminal")) {
                        c_token++;
                        if (END_OF_COMMAND)
                                list_terms();
                        else
                                term = set_term(c_token);
                        screen_ok = FALSE;
                        c_token++;
                }
                else if (almost_equals(c_token,"sa$mples")) {
                        c_token++;
                        tsamp = (int)magnitude(const_express(&a));
                        if (tsamp < 1)
                                int_error("sampling rate must be > 0; sampling unchanged",
                                        c_token);
                        else {
                                samples = tsamp;
                                pointmem(samples);
                        }
                }
                else if (almost_equals(c_token,"o$utput")) {
                        c_token++;
                        if (END_OF_COMMAND) {   /* no file specified */
                                (void) fclose(outfile);
                                outfile = fdopen(dup(STDOUT), "w");
                                term_init = FALSE;
                                (void) strcpy(outstr,"STDOUT");
                        } else if (!isstring(c_token)) 
                                int_error("expecting filename",c_token);
                        else {
                                quote_str(sv_file,c_token);
                                if (!(f = fopen(sv_file,"w"))) {
                                  os_error("cannot open file; output not changed",c_token);
                                }
                                (void) fclose(outfile);
                                outfile = f;
                                term_init = FALSE;
                                outstr[0] = '\'';
                                strcat(strcpy(outstr+1,sv_file),"'");
                        } 
                        c_token++;
                }
                else if (almost_equals(c_token,"a$utoscale")) {
                        autoscale = TRUE;
                        c_token++;
                } 
                else if (almost_equals(c_token,"noa$utoscale")) {
                        autoscale = FALSE;
                        c_token++;
                } 
                else if (almost_equals(c_token,"nol$ogscale")) {
                        log_x = log_y = FALSE;
                        c_token++;
                }
                else if (almost_equals(c_token,"z$ero")) {
                        c_token++;
                        zero = magnitude(const_express(&a));
                }
                else if (almost_equals(c_token,"x$range")) {
                        c_token++;
                        if (!equals(c_token,"["))
                                int_error("expecting '['",c_token);
                        c_token++;
                        load_range(&xmin,&xmax);
                        if (!equals(c_token,"]"))
                                int_error("expecting ']'",c_token);
                        c_token++;
                }
                else if (almost_equals(c_token,"y$range")) {
                        c_token++;
                        if (!equals(c_token,"["))
                                int_error("expecting '['",c_token);
                        c_token++;
                        load_range(&ymin,&ymax);
                        autoscale = FALSE;
                        if (!equals(c_token,"]"))
                                int_error("expecting ']'",c_token);
                        c_token++;
                }
                else if (almost_equals(c_token,"l$ogscale")) {
                        c_token++;
                        if (equals(c_token,"x")) {
                                log_y = FALSE;
                                log_x = TRUE;
                                c_token++;
                        }
                        else if (equals(c_token,"y")) {
                                log_x = FALSE;
                                log_y = TRUE;
                                c_token++;
                        }
                        else if (equals(c_token,"xy") || equals(c_token,"yx")) {
                                log_x = log_y = TRUE;
                                c_token++;
                        }
                        else
                                int_error("expecting 'x', 'y', or 'xy'",c_token);
                }
                else if (almost_equals(c_token,"d$ata")) {
                        c_token++;
                        if (!almost_equals(c_token,"s$tyle"))
                                int_error("expecting keyword 'style'",c_token);
                        c_token++;
                        if (almost_equals(c_token,"l$ines")) {
                                data_style = LINES;
                        }
                        else if (almost_equals(c_token,"i$mpulses")) {
                                data_style = IMPULSES;
                        }
                        else if (almost_equals(c_token,"p$oints")) {
                                data_style = POINTS;
                        }
                        else
                                int_error("expecting 'lines', 'points', or 'impulses'",c_token);
                        c_token++;
                }
                else if (almost_equals(c_token,"f$unction")) {
                        c_token++;
                        if (!almost_equals(c_token,"s$tyle"))
                                int_error("expecting keyword 'style'",c_token);
                        c_token++;
                        if (almost_equals(c_token,"l$ines")) {
                                func_style = LINES;
                        }
                        else if (almost_equals(c_token,"i$mpulses")) {
                                func_style = IMPULSES;
                        }
                        else if (almost_equals(c_token,"p$oints")) {
                                func_style = POINTS;
                        }
                        else
                                int_error("expecting 'lines', 'points', or 'impulses'",c_token);
                        c_token++;
                }
                else
                        int_error("unknown set option (try 'help set')",c_token);
        }
        else if (almost_equals(c_token,"sh$ow")) {
                if (almost_equals(++c_token,"f$unctions")) {
                        c_token++;
                        if (almost_equals(c_token,"s$tyle"))  {
                                fprintf(stderr,"\nfunctions are plotted with ");
                                switch (func_style) {
                                        case LINES: fprintf(stderr,"lines.\n\n"); break;
                                        case POINTS: fprintf(stderr,"points.\n\n"); break;
                                        case IMPULSES: fprintf(stderr,"impulses.\n\n"); break;
                                }
                        screen_ok = FALSE;
                        c_token++;
                        }
                        else
                                view_functions();
                }
                else if (almost_equals(c_token,"v$ariables")) {
                        view_variables();
                        c_token++;
                }
                else if (almost_equals(c_token,"ac$tion_table") ||
                                 equals(c_token,"at") ) {
                        c_token++;
                        view_at();
                        c_token++;
                } 
                else if (almost_equals(c_token,"d$ata")) {
                        c_token++;
                        if (!almost_equals(c_token,"s$tyle"))
                                int_error("expecting keyword 'style'",c_token);
                        fprintf(stderr,"\ndata is plotted with ");
                        switch (data_style) {
                                case LINES: fprintf(stderr,"lines.\n\n"); break;
                                case POINTS: fprintf(stderr,"points.\n\n"); break;
                                case IMPULSES: fprintf(stderr,"impulses.\n\n"); break;
                        }
                        screen_ok = FALSE;
                        c_token++;
                } 
                else if (almost_equals(c_token,"x$range")) {
                        fprintf(stderr,"\nxrange is [%g : %g]\n\n",xmin,xmax);
                        screen_ok = FALSE;
                        c_token++;
                } 
                else if (almost_equals(c_token,"y$range")) {
                        fprintf(stderr,"\nyrange is [%g : %g]\n\n",ymin,ymax);
                        screen_ok = FALSE;
                        c_token++;
                } 
                else if (almost_equals(c_token,"z$ero")) {
                        fprintf(stderr,"\nzero is %g\n\n",zero);
                        screen_ok = FALSE;
                        c_token++;
                }
                else if (almost_equals(c_token,"sa$mples")) {
                        fprintf(stderr,"\nsampling rate is %d\n\n",samples);
                        screen_ok = FALSE;
                        c_token++;
                }
                else if (almost_equals(c_token,"o$utput")) {
                        fprintf(stderr,"\noutput is sent to %s\n\n",outstr);
                        screen_ok = FALSE;
                        c_token++;
                }
                else if (almost_equals(c_token,"t$erminal")) {
                        fprintf(stderr,"\nterminal type is %s\n\n",term_tbl[term].name);
                        screen_ok = FALSE;
                        c_token++;
                }
                else if (almost_equals(c_token,"au$toscale")) {
                        fprintf(stderr,"\nautoscaling is %s\n\n",(autoscale)? "ON" : "OFF");
                        screen_ok = FALSE;
                        c_token++;
                }
                else if (almost_equals(c_token,"ve$rsion")) {
                        (void) putc('\n',stderr);
                        show_version();
                        c_token++;
                } 
                else if (almost_equals(c_token,"l$ogscale")) {
                        if (log_x && log_y)
                                fprintf(stderr,"\nlogscaling both x and y axes\n\n");
                        else if (log_x)
                                fprintf(stderr,"\nlogscaling x axis\n\n");
                        else if (log_y)
                                fprintf(stderr,"\nlogscaling y axis\n\n");
                        else
                                fprintf(stderr,"\nno logscaling\n\n");
                        c_token++;
                }
                else if (almost_equals(c_token,"a$ll")) {
                        c_token++;
                        (void) putc('\n',stderr);
                        show_version();
                        fprintf(stderr,"data is plotted with ");
                        switch (data_style) {
                                case LINES: fprintf(stderr,"lines.\n"); break;
                                case POINTS: fprintf(stderr,"points.\n"); break;
                                case IMPULSES: fprintf(stderr,"impulses.\n"); break;
                        }
                        fprintf(stderr,"functions are plotted with ");
                        switch (func_style) {
                                case LINES: fprintf(stderr,"lines.\n"); break;
                                case POINTS: fprintf(stderr,"points.\n"); break;
                                case IMPULSES: fprintf(stderr,"impulses.\n"); break;
                        }
                        fprintf(stderr,"output is sent to %s\n",outstr);
                        fprintf(stderr,"terminal type is %s\n",term_tbl[term].name);
                        fprintf(stderr,"sampling rate is %d\n\n",samples);
                        if (log_x && log_y)
                                fprintf(stderr,"logscaling both x and y axes\n");
                        else if (log_x)
                                fprintf(stderr,"logscaling x axis\n");
                        else if (log_y)
                                fprintf(stderr,"logscaling y axis\n");
                        else
                                fprintf(stderr,"no logscaling\n");
                        fprintf(stderr,"autoscaling is %s\n",(autoscale)? "ON" : "OFF");
                        fprintf(stderr,"zero is %g\n",zero);
                        fprintf(stderr,"xrange is [%g : %g]\n",xmin,xmax);
                        fprintf(stderr,"yrange is [%g : %g]\n",ymin,ymax);
                        view_variables();
                        view_functions();
                        c_token++;
                }
                else
                        int_error("unknown show option (try 'help show')",c_token);
        }
        else if (almost_equals(c_token,"cl$ear")) { /* now does clear screen! */
                if (!term_init) {
                        (*term_tbl[term].init)();
                        term_init = TRUE;
                }
                (*term_tbl[term].graphics)();
                (*term_tbl[term].text)();
                (void) fflush(outfile);
                screen_ok = FALSE;
                c_token++;
        }
        else if (almost_equals(c_token,"she$ll")) {
                do_shell();
                screen_ok = FALSE;
                c_token++;
        }
        else if (almost_equals(c_token,"sa$ve")) {
                if (almost_equals(++c_token,"f$unctions")) {
                        if (!isstring(++c_token))
                                int_error("expecting filename",c_token);
                        else {
                                quote_str(sv_file,c_token);
                                save_functions(fopen(sv_file,"w"));
                        }
                }
                else if (almost_equals(c_token,"v$ariables")) {
                        if (!isstring(++c_token))
                                int_error("expecting filename",c_token);
                        else {
                                quote_str(sv_file,c_token);
                                save_variables(fopen(sv_file,"w"));
                        }
                }
                else if (isstring(c_token)) {
                                quote_str(sv_file,c_token);
                                save_all(fopen(sv_file,"w"));
                }
                else {
                        int_error(
                "filename or keyword 'functions' or 'variables' expected",c_token);
                }
                c_token++;
        }
        else if (almost_equals(c_token,"l$oad")) {
                if (!isstring(++c_token))
                        int_error("expecting filename",c_token);
                else {
                        quote_str(sv_file,c_token);
                        load_file(fopen(sv_file,"r"));
                }
        /* input_line[] and token[] now destroyed! */
        }
        else if (almost_equals(c_token,"ex$it") ||
                almost_equals(c_token,"q$uit")) {
                done(IO_SUCCESS);
        }
        else if (!equals(c_token,";")) {                /* null statement */
                int_error("invalid command",c_token);
        }
}


load_range(a,b)
double *a,*b;
{
struct value t;

        if (equals(c_token,"]"))
                return;
        if (END_OF_COMMAND) {
            int_error("starting range value or 'to' expected",c_token);
        } else if (!equals(c_token,"to") && !equals(c_token,":"))  {
                *a = real(const_express(&t));
        }       
        if (!equals(c_token,"to") && !equals(c_token,":"))
                int_error("Keyword 'to' or ':' expected",c_token);
        c_token++;
        if (!equals(c_token,"]"))
                *b = real(const_express(&t));
}


plotrequest()
{

        dummy_var[0] = 'x';  /* default */
        dummy_var[1] = '\0';
        if (equals(c_token,"[")) {
                c_token++;
                if (isletter(c_token)) {
                        copy_str(dummy_var,c_token++);
                        if (equals(c_token,"="))
                                c_token++;
                        else
                                int_error("'=' expected",c_token);
                }
                load_range(&xmin,&xmax);
                if (!equals(c_token,"]"))
                        int_error("']' expected",c_token);
                c_token++;
        }

        if (equals(c_token,"[")) { /* set optional y ranges */
                c_token++;
                load_range(&ymin,&ymax);
                autoscale = FALSE;
                if (!equals(c_token,"]"))
                        int_error("']' expected",c_token);
                c_token++;
        }

        eval_plots();
}


define()
{
register int value,start_token;  /* start_token is the 1st token in the */
                                                                /* function definition.                 */

        if (equals(c_token+1,"(")) {
                /* function ! */
                start_token = c_token;
                copy_str(dummy_var, c_token + 2);
                c_token += 5; /* skip (, dummy, ) and = */
                value = c_function = user_defined(start_token);
                build_at(&(udft[value].at));
                                /* define User Defined Function (parse.c)*/
                capture(udft[value].definition,start_token,c_token-1);
        }
        else {
                /* variable ! */
                c_token +=2;
                (void) const_express(&vt[value = add_value(c_token - 2) ].vt_value);
                vt[value].vt_undef = FALSE;
        }
}


#define iscomment(c) (c == '!' || c == '#')

get_data(plot_num)
int plot_num;
{
static char data_file[MAX_ID_LEN+1], line[MAX_LINE_LEN+1];
register int i, l_num;
register FILE *fp;
float x, y;

        quote_str(data_file, c_token);
        plot[plot_num].plot_type = DATA;
        if (!(fp = fopen(data_file, "r")))
                os_error("can't open data file", c_token);

        l_num = 0;

        i = 0;
        while (fgets(line, MAX_LINE_LEN, fp)) {
                l_num++;
                if (iscomment(line[0]) || ! line[1])    /* line[0] will be '\n' */
                        continue;               /* ignore comments  and blank lines */
                switch (sscanf(line, "%f %f", &x, &y)) {
                        case 1:                 /* only one number on the line */
                                y = x;          /* assign that number to y */
                                x = i;          /* and make the index into x */
                        /* no break; !!! */
                        case 2:
                                if (x >= xmin && x <= xmax && (autoscale ||
                                        (y >= ymin && y <= ymax))) {
                                        if (log_x) {
                                                if (x <= 0.0)
                                                        break;
                                                plot[plot_num].points[i].x = log10(x);
                                        } else
                                                plot[plot_num].points[i].x = x;
                                        if (log_y) {
                                                if (y <= 0.0)
                                                        break;
                                                plot[plot_num].points[i].y = log10(y);
                                        } else
                                                plot[plot_num].points[i].y = y;
                                        if (autoscale) {
                                                if (y < ymin) ymin = y;
                                                if (y > ymax) ymax = y;
                                        }
                                        i++;
                                }
                                break;
                        default:
                                (void) sprintf(line, "bad data on line %d", l_num);
                                int_error(line,c_token);
                }
        }
        plot[plot_num].count = i;
}


eval_plots()
{
register int i, plot_num, start_token, mysamples;
register double x_min, x_max, y_min, y_max, x;
register double xdiff, temp;
struct value a;

        /* don't sample higher than output device can handle! */
        mysamples = (samples <= term_tbl[term].xmax) ?samples :term_tbl[term].xmax;

        if (log_x) {
                if (xmin < 0.0 || xmax < 0.0)
                        int_error("x range must be greater than 0 for log scale!",NO_CARET);
                x_min = log10(xmin);
                x_max = log10(xmax);
        } else {
                x_min = xmin;
                x_max = xmax;
        }

        if (autoscale) {
                ymin = HUGE;
                ymax = -HUGE;
        } else if (log_y && (ymin <= 0.0 || ymax <= 0.0))
                        int_error("y range must be greater than 0 for log scale!",
                                NO_CARET);

        xdiff = (x_max - x_min) / mysamples;

        c_function = MAX_UDFS;          /* last udft[] entry used for plots */

        plot_num = 0;

        while (TRUE) {
                if (END_OF_COMMAND)
                        int_error("function to plot expected",c_token);
                if (plot_num == MAX_PLOTS || plot[plot_num].points == NULL)
                        int_error("maximum number of plots exceeded",NO_CARET);

                start_token = c_token;

                if (is_definition(c_token)) {
                        define();
                } else {
                        if (isstring(c_token)) {                        /* data file to plot */
                                plot[plot_num].plot_type = DATA;
                                plot[plot_num].plot_style = data_style;
                                get_data(plot_num);
                                c_token++;
                        } 
                        else {                                                  /* function to plot */
                                plot[plot_num].plot_type = FUNC;
                                plot[plot_num].plot_style = func_style;
                                build_at(&udft[MAX_UDFS].at);

                                for (i = 0; i <= mysamples; i++) {
                                        if (i == samples+1)
                                                int_error("number of points exceeded samples",
                                                        NO_CARET);
                                        x = x_min + i*xdiff;
                                        if (log_x)
                                                x = pow(10.0,x);
                                        (void) complex(&udft[MAX_UDFS].dummy_value, x, 0.0);

                                        evaluate_at(&udft[MAX_UDFS].at,&a);

                                        if (plot[plot_num].points[i].undefined =
                                                undefined || (fabs(imag(&a)) > zero))
                                                        continue;

                                        temp = real(&a);

                                        if (log_y && temp <= 0.0) {
                                                        plot[plot_num].points[i].undefined = TRUE;
                                                        continue;
                                        }
                                        if (autoscale) {
                                                if (temp < ymin) ymin = temp;
                                                if (temp > ymax) ymax = temp;
                                        } else if (temp < ymin || temp > ymax) {
                                                plot[plot_num].points[i].undefined = TRUE;
                                                continue;
                                        }

                                        plot[plot_num].points[i].y = log_y ? log10(temp) : temp;
                                }
                                plot[plot_num].count = i; /* mysamples + 1 */
                        }
                        capture(plot[plot_num].title,start_token,c_token-1);
                        if (almost_equals(c_token,"w$ith")) {
                                c_token++;
                                if (almost_equals(c_token,"l$ines")) {
                                        plot[plot_num].plot_style = LINES;
                                } 
                                else if (almost_equals(c_token,"i$mpulses")) {
                                        plot[plot_num].plot_style = IMPULSES;
                                } 
                                else if (almost_equals(c_token,"p$oints")) {
                                        plot[plot_num].plot_style = POINTS;
                                } 
                                else
                                        int_error("expecting 'lines', 'points', or 'impulses'",
                                        c_token);
                                c_token++;
                        }
                        plot_num++;
                }

                if (equals(c_token,","))
                        c_token++;
                else
                        break;
        }

        if (autoscale && (ymin == ymax))
                ymax += 1.0;    /* kludge to avoid divide-by-zero in do_plot */

        if (log_y) {
                y_min = log10(ymin);
                y_max = log10(ymax);
        } else {
                y_min = ymin;
                y_max = ymax;
        }
        do_plot(plot,plot_num,x_min,x_max,y_min,y_max);
}



done(status)
int status;
{
        if (term)
                (*term_tbl[term].reset)();
        exit(status);
}

#ifdef vms
do_shell()
{
        if ((vaxc$errno = lib$spawn()) != SS$_NORMAL) {
                os_error("spawn error");
        }
}

#else /* vms */

#ifdef MSDOS

do_shell()
{
register char *comspec;
        if (!(comspec = getenv("COMSPEC")))
                comspec = "\command.com";
        if (spawnl(P_WAIT,comspec,NULL))
                os_error("unable to spawn shell");
}

#else /* MSDOS */

#ifdef VFORK

do_shell()
{
register char *shell;
register int p;
static int execstat;
        if (!(shell = getenv("SHELL")))
                shell = SHELL;
        if ((p = vfork()) == 0) {
                execstat = execl(shell,shell,NULL);
                _exit(1);
        } else if (p == -1)
                os_error("vfork failed",c_token);
        else
                while (wait(NULL) != p)
                        ;
        if (execstat == -1)
                os_error("shell exec failed",c_token);
        (void) putc('\n',stderr);
}
#else /* VFORK */

#define EXEC "exec "
do_shell()
{
static char exec[100] = EXEC;
register char *shell;
        if (!(shell = getenv("SHELL")))
                shell = SHELL;

        if (system(strcpy(&exec[sizeof(EXEC)-1],shell)))
                os_error("system() failed",NO_CARET);

        (void) putc('\n',stderr);
}
#endif /* VFORK */
#endif /* MSDOS */
#endif /* vms */
