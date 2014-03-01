# Copyright (c) 2014 Stanford University
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

require('plyr')
require('ggplot2')
require('scales')

# turn raw number of bytes into human readable
# will round if you don't pass in powers of two
humanbytes <- function(x) {
  ifelse(x < 1024,
         x,
         ifelse(x < 1024 * 1024,
                sprintf('%0.0fK', x / 1024),
                sprintf('%0.0fM', x / 1024 / 1024)))
}

# read results, take the minimum time of any offset and run
results <- read.csv('results.csv')
results$usperop <- results$seconds/results$count*1e6
best <- ddply(results,
              c('machine', 'disk', 'writecache', 'size', 'direct'),
              summarize, usperop=min(usperop))

# create plot
g <- ggplot(best[best$direct=='no',], # direct=yes gets too crowded
            aes(x=size,
                y=usperop/1000,
                color=disk,
                group=interaction(machine, disk, writecache, direct))) +
     geom_point(aes(shape=machine),
                size=3) +
     geom_line(aes(linetype=writecache), size=.75) +
     coord_cartesian(xlim=(c(1, 2**21)*3/4),
                     ylim=c(.05, 500)) +
                     #linear: ylim=(c(0, 30000) + c(-1,1)*500)) +
     scale_x_continuous(breaks=2**(0:20),
                        trans=log_trans(2),
                        labels=humanbytes) +
     scale_y_continuous(breaks=c(.01, .1, 1, 10, 100),
                        minor_breaks=NULL,
                        trans=log_trans(10)) +
     annotation_logticks(base=10, sides='l') +
     #linear: scale_y_continuous(breaks=.1:10*3000) +
     xlab('size per write (bytes)') +
     ylab('time per write+fdatasync (milliseconds)')
