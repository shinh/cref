#!/usr/bin/env ruby

require 'rubygems'
require 'cgi'
require 'json'

FILENAMES = %w(
libc-2.18-x64.json
libc-2.18-i686.json
libc-2.17-x32.json
libc-2.9-nacl-x64.json
libc-2.9-nacl-i686.json
)

all_infos = []
all_types = {}
FILENAMES.each do |filename|
  json = JSON.load(File.read(filename))
  types = {}
  all_infos << types
  json.each do |cu|
    cu_type = cu['type']
    cu_type.each do |name, info|
      all_types[name] = 1

      if info[0] == 'struct' && info[1] == 0
        next
      end

      if types[name]
        if (name == '_IO_FILE' || name == '_IO_FILE_plus' ||
            name == 'helper_file' || name == 'locked_FILE' ||
            name == 'group' || name == 'area' || name == 'ct_data' ||
            name == 'known_object' || name == '???')
          types[name][1] = [types[name][1], info[1]].max
        elsif info != types[name]
          raise "#{name} #{info} vs #{types[name]}"
          end
      else
        types[name] = info
      end
    end
  end
end

print "sizeof(XXX)\t"

#3.times do |ti|
#  puts %Q(<h2>#{['basic types', 'typedefs', 'structs'][ti]}</h2>
#<table border=1>
#<th><td>#{FILENAMES.map{|fn|'<td>' + fn.sub(/\.json/, '')} * ''}</th>
#)
#end

puts %Q(#{FILENAMES.map{|fn|fn.sub(/\.json/, '')} * "\t"})

all_types.sort_by{|name, _|name == '???' ? '~~~' : name.upcase}.each do |name, _|
  tr = "#{name}"
  all_infos.each do |types|
    info = types[name]
    if !info
      tr += "\t???"
      next
    end

    case info[0]
    when 'base'
      tr += "\t#{info[1]} (basic)"
    when 'struct'
      tr += "\t#{info[1]} (struct)"
    when 'typedef'
      real = info
      while real && real[0] == 'typedef'
        real = types[real[1] =~ /(\*|\[\])$/ ? 'intptr_t' : real[1]]
      end
      if real
        size = real[1]
      else
        size = '???'
      end
      tr += "\t#{size} (#{info[1]})"
    end
  end
  puts tr
end

