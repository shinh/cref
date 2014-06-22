#!/usr/bin/env ruby

require 'cgi'

TARGETS = %w(
libc-2.18-x64
libc-2.18-i686
libc-2.17-x32
libc-2.9-nacl-x64
libc-2.9-nacl-i686
)

all_macros = {}

TARGETS.each do |target|
  all_macros[target] = {} if !all_macros[target]
  File.readlines(target + '.txt').each do |line|
    line.chomp!
    if /^#define (\S+) (.*)/ !~ line
      raise line
    end
    all_macros[target][$1] = $2
  end
end

print "C macros\t"

puts %Q(#{TARGETS * "\t"})

all_macros[TARGETS[0]].sort.each do |name, _|
  tr = "#{name}"
  TARGETS.each do |target|
    value = all_macros[target][name]
    if !value
      value = '???'
    end
    tr += "\t" + CGI.escapeHTML(value)
  end
  puts tr
end

