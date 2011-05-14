/*
    Embedded Ruby interpreter module for C-ICAP server
    Copyright (C) 2009  Roman Shterenzon

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <ruby.h>
#undef PACKAGE_NAME
#undef PACKAGE_TARNAME
#undef PACKAGE_STRING
#undef PACKAGE_VERSION

#include "autoconf.h"
#include "c-icap.h"
#include "service.h"
#include "module.h"
#include "header.h"
#include "body.h"
#include "simple_api.h"
#include "debug.h"

#include <pthread.h>
#include <ruby/encoding.h>
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

/* TODO: stack growth direction */
static void pthread_get_stack(void **stack_begin, void **stack_end) {
	size_t stack_size;
#ifdef HAVE_PTHREAD_GET_STACKADDR_NP /* MacOS X */
	pthread_t t_id = pthread_self();
	*stack_begin = pthread_get_stackaddr_np(t_id);
	stack_size = pthread_get_stacksize_np(t_id);
#elif defined(HAVE_PTHREAD_GETATTR_NP) /* Linux */
	pthread_attr_t attr;
	pthread_getattr_np(pthread_self(), &attr);
	pthread_attr_getstack(&attr, stack_begin, &stack_size);
#else
	#error "Cannot figure how to get pthread stack parameters"
#endif
	*stack_end = *stack_begin + stack_size;
}


struct ruby_vars {
	VALUE svc_inst;	// instance of Ruby class
};


int init_ruby_handler(struct ci_server_conf *server_conf);
ci_service_module_t *load_ruby_module(char *service_file);


CI_DECLARE_DATA service_handler_module_t module = {
     "ruby_handler",
     ".rb",
     init_ruby_handler,
     NULL,                      /*post_init .... */
     NULL,                      /*release handler */
     load_ruby_module,
     NULL
};


int ruby_init_service(ci_service_xdata_t *srv_xdata,
                      struct ci_server_conf *server_conf);
void ruby_close_service();
void *ruby_init_request_data(ci_request_t *);
void ruby_release_request_data(void *data);
void replace_headers(ci_request_t *req);

int ruby_check_preview_handler(char *preview_data, int preview_data_len,
                               ci_request_t *);
int ruby_end_of_data_handler(ci_request_t *);
int ruby_service_io(char *rbuf, int *rlen, char *wbuf, int *wlen, int iseof,
                    ci_request_t *req);


typedef struct protect_call_arg {
    VALUE recv;
    ID mid;
    int argc;
    VALUE *argv;
} protect_call_arg_t;

static VALUE protect_funcall0(VALUE arg)
{
    return rb_funcall2(((protect_call_arg_t *) arg)->recv,
		       ((protect_call_arg_t *) arg)->mid,
		       ((protect_call_arg_t *) arg)->argc,
		       ((protect_call_arg_t *) arg)->argv);
}

VALUE rb_protect_funcall(VALUE recv, ID mid, int *state, int argc, ...)
{
    va_list ap;
    VALUE *argv;
    struct protect_call_arg arg;

    if (argc > 0) {
	int i;

	argv = ALLOCA_N(VALUE, argc);

	va_start(ap, argc);
	for (i = 0; i < argc; i++) {
	    argv[i] = va_arg(ap, VALUE);
	}
	va_end(ap);
    }
    else {
	argv = 0;
    }
    arg.recv = recv;
    arg.mid = mid;
    arg.argc = argc;
    arg.argv = argv;
    return rb_protect(protect_funcall0, (VALUE) &arg, state);
}


int init_ruby_handler(struct ci_server_conf *server_conf)
{
	ci_debug_printf(9, "init_ruby_handler()\n");
	return 0;
}

VALUE klass;
ci_service_module_t *load_ruby_module(char *service_file)
{
	ci_service_module_t *service = NULL;

	service = malloc(sizeof(ci_service_module_t));

#ifdef HAVE_LOCALE_H
	setlocale(LC_ALL, "");
#endif
	void *stack_begin, *stack_end;

	pthread_get_stack(&stack_begin, &stack_end);
	ruby_bind_stack(stack_begin, stack_end);

	RUBY_INIT_STACK;
	ruby_init();
	ruby_init_loadpath();
	ruby_script(service_file);
	rb_define_module("Gem");
	rb_gc_disable();
	void Init_prelude(void);
	Init_prelude();

	int error;
	rb_protect((VALUE (*)(VALUE))rb_require, (VALUE)service_file, &error);

	if (error) {
		ci_debug_printf(1, "Error loading %s\n", service_file);
		free(service);
		return NULL;
	}

	klass = rb_protect((VALUE (*)(VALUE))rb_path2class, (VALUE)"IcapService", &error);
	if (error) {
		ci_debug_printf(1, "Error: Ruby class IcapService undefined\n");
		free(service);
		return NULL;
	}
	service->mod_data = NULL;
	service->mod_conf_table = NULL;
	service->mod_init_service = ruby_init_service;
	service->mod_post_init_service = NULL;
	service->mod_close_service = ruby_close_service;
	service->mod_init_request_data = ruby_init_request_data;
	service->mod_release_request_data = ruby_release_request_data;
	service->mod_check_preview_handler = ruby_check_preview_handler;
	service->mod_end_of_data_handler = ruby_end_of_data_handler;
	service->mod_service_io = ruby_service_io;

	service->mod_name = strdup(service_file);
	service->mod_type = ICAP_REQMOD | ICAP_RESPMOD;

	ci_debug_printf(1, "OK service %s loaded\n", service_file);
	return service;
}


int ruby_init_service(ci_service_xdata_t *srv_xdata,
                      struct ci_server_conf *server_conf)
{
	ci_debug_printf(9, "ruby_init_service()\n");

	/*Tell to the icap clients that we can support up to 1024 size of preview data*/
	ci_service_set_preview(srv_xdata, 1024);

	/*Tell to the icap clients that we support 204 responses*/
	ci_service_enable_204(srv_xdata);

	/*Tell to the icap clients to send preview data for all files*/
	ci_service_set_transfer_preview(srv_xdata, "*");

	return 0;
}


void ruby_close_service()
{
	ci_debug_printf(9, "ruby_close_service()\n");
}


#define GC_EVERY 10
void *ruby_init_request_data(ci_request_t *req)
{
	int i;
	ci_headers_list_t *hdrs;
	struct ruby_vars *rvars;
	rvars = (struct ruby_vars*)malloc(sizeof(struct ruby_vars));

	static int req_count = 0;
	if ( req_count >= GC_EVERY ) {
		ci_debug_printf(8, "Starting GC\n");
		req_count = 0;
		rb_gc_enable();
		rb_gc_start();
		rb_gc_disable();
	}
	req_count ++;

	VALUE reqheaders = rb_ary_new2(10);
	VALUE respheaders = rb_ary_new2(10);
	
	hdrs = ci_http_request_headers(req);
	if (hdrs) {
		for (i = 0; i < hdrs->used; i++) {
			rb_ary_push(reqheaders, rb_str_new2(hdrs->headers[i]));
		}
	}

	hdrs = ci_http_response_headers(req);
	if (hdrs) {
		for (i = 0; i< hdrs->used; i++) {
			rb_ary_push(respheaders, rb_str_new2(hdrs->headers[i]));
		}
	}

	VALUE args[2] = {reqheaders, respheaders};

	rvars->svc_inst = rb_class_new_instance(2, args, klass);

	return (void *)rvars;
}


void ruby_release_request_data(void *data)
{
	struct ruby_vars *rvars = (struct ruby_vars *)data;
	ci_debug_printf(9, "ruby_release_request_data()\n");
	free(rvars);
}

int ruby_check_preview_handler(char *preview_data, int preview_data_len,
                               ci_request_t *req)
{
	struct ruby_vars *rvars = ci_service_data(req);
	int error;

	/* If there are not body data in HTTP encapsulated object but only headers
	 * respond with Allow204 (no modification required) and terminate here the
	 * ICAP transaction */
	if(!ci_req_hasbody(req)) {
		ci_debug_printf(9, "No body, no modification will be performed\n");
		return CI_MOD_ALLOW204;
	}

	/* check if the service can do interleaved r/w */
	VALUE sym = rb_intern("async?");
	if ( rb_respond_to(rvars->svc_inst, sym) &&
		rb_protect_funcall(rvars->svc_inst, sym, &error, 0) == Qtrue ) {
		/* Unlock the request body data so the c-icap server can send data
		before all body data has received */
		ci_req_unlock_data(req);
	}

	/* If it's not 200 OK, then no modification is required */
	ci_headers_list_t *heads =  ci_http_response_headers(req);
	if ( !strstr(heads->headers[0], "200") ) {
		ci_debug_printf(9, "Not HTTP/1.x 200, no modification will be performed\n");
		return CI_MOD_ALLOW204;
	}

	sym = rb_intern("preview");
	/* Check if the IcapService#preview method exists */
	if (rb_respond_to(rvars->svc_inst, sym)) {
		VALUE ret = rb_protect_funcall(rvars->svc_inst, sym, &error, 1, rb_str_new(preview_data, preview_data_len));
		if (error) {
			ci_debug_printf(1, "Error: IcapService#preview raised an exception.\n");
			return CI_MOD_ERROR;
		} else {
			return FIX2INT(ret);
		}
	/* if there's no preview method, then just pass the data chunk to IcapService#read */
	} else {
		ci_debug_printf(9, "Calling ruby_service_io with preview data\n");
		ruby_service_io(NULL, NULL, preview_data, &preview_data_len, 0, req);
	}
	return CI_MOD_CONTINUE;
}

int ruby_end_of_data_handler(ci_request_t *req)
{
	ci_debug_printf(9, "ruby_end_of_data_handler()\n");
	struct ruby_vars *rvars = ci_service_data(req);
	rb_protect_funcall(rvars->svc_inst, rb_intern("end_of_data"), NULL, 0);
	
	if (!ci_req_sent_data(req)) {
		replace_headers(req);
	}
	return CI_MOD_DONE;
}

/* Replaces the original headers with the headers returned by the Ruby code */
void replace_headers(ci_request_t *req)
{
	struct ruby_vars *rvars = ci_service_data(req);
	int error;
	VALUE respheaders = rb_protect_funcall(rvars->svc_inst, rb_intern("respheaders"), &error, 0);
	if (error) {
		ci_debug_printf(1, "Error: Ruby IcapService#respheaders raised an exception.\n");
	} else {
		if (TYPE(respheaders) != T_ARRAY) {
			ci_debug_printf(1, "Error: IcapService#respheaders should return an Array.\n");
			return;
		}
		/* Replace all headers */
		ci_http_response_reset_headers(req);
		int i;
		VALUE hdr;
		for (i=0; i<RARRAY_LEN(respheaders); i++) {
			hdr = StringValue((RARRAY_PTR(respheaders)[i]));
			ci_debug_printf(9, "adding header: %s\n", RSTRING_PTR(hdr));
			ci_http_response_add_header(req, RSTRING_PTR(hdr));
	    }
	}
}

int ruby_service_io(char *wbuf, int *wlen, char *rbuf, int *rlen, int iseof,
                    ci_request_t *req)
{
	int ret = CI_OK;
	struct ruby_vars *rvars = ci_service_data(req);
	ci_debug_printf(9, "ruby_service_io()\n");


	if (rlen && rbuf) {
		if (*rlen < 0) {
			ret = CI_ERROR;
		} else {
			rb_protect_funcall(rvars->svc_inst, rb_intern("read_data"), NULL, 1,
								rb_enc_str_new(rbuf, *rlen, rb_ascii8bit_encoding()));
		}
	}

	if (wlen && wbuf) {
		VALUE data = rb_protect_funcall(rvars->svc_inst, rb_intern("write_data"), NULL, 1,
							INT2FIX(*wlen));
		VALUE str = StringValue(data);
		if ( RSTRING_LEN(str) > *wlen ) {
			ci_debug_printf(1, "Error: write_data() returned more than requested: %ld for %d", RSTRING_LEN(str), *wlen);
		} else {
			*wlen = RSTRING_LEN(str);
		}
		if (*wlen == 0) {
			*wlen = CI_EOF;
		}
		else {
			memcpy(wbuf, RSTRING_PTR(str), (*wlen)*sizeof(char));
		}
	}

	return ret;
}
