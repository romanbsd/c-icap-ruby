Embedded Ruby 1.9 interpreter for C-ICAP server
===============================================

This module allows writing services for the [c-icap][1] server in pure Ruby.

It was tested with 1.9.1p378 patched with the bind_ruby_stack() patch (see below).

*This is an experimental and not a production grade software*

## Required patch

It wasn't used since the days of 1.9.1, so there's a slight chance that
1.9.2 will work without the patch, but I seriously doubt it.

You can read about the patch here:
http://redmine.ruby-lang.org/issues/show/2294

The patch for 1.9.1p378 cab be found here:
http://redmine.ruby-lang.org/attachments/download/651/ruby_bind_stack.patch

The patch for 1.9.2p0 can be found here:
http://redmine.ruby-lang.org/attachments/download/1153/ruby_bind_stack_1.9.2p0.patch

## Usage

In c-icap.conf:

    Service foo /some/path/foo.rb

See examples/icap.rb for better understanding what should be implemented by a service.
You will need to call .force_encoding('ASCII-8BIT') on the response body, unless it's
already in ASCII-8BIT encoding.

## Bugs

Even with the patch I'm not sure that it's possibe to embed several Ruby interpreters in one process.
You may want to limit the number of worker threads to 1 (ThreadsPerChild 1) and
increase the number of c-icap processes.

[1]: http://c-icap.sourceforge.net/

Copyright (c) 2009 Roman Shterenzon
