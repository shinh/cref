#!/usr/bin/env ruby

require 'cgi'

header = $<.gets.split("\t")
puts "<h1>#{header[0]}</h1>"

puts %Q(<table border=1>
<th>#{header[1..-1].map{|h|'<td>' + h} * ''}</th>
)

$<.each do |line|
  r = line.split("\t")
  tr = "<tr><th>#{r[0]}"
  r[1..-1].each do |c|
    tr += "<td>" + CGI.escapeHTML(c)
  end
  tr += '</tr>'
  puts tr
end

puts "</table>"


