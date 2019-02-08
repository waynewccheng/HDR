% 2-5-2019
% generate a .bat for running HDRDisplay
% usage: hdr_pattern_gen_command_line('test.m',0.01:0.01:0.05)

function hdr_pattern_gen_command_line (fn_command, range)

str = 'HDRDisplay -hdr -fullscreen';
for i = range
    new_term = filename_gen(i);
    str = sprintf('%s %s.hdr',str,new_term);
end
str

fid = fopen(fn_command,'w');
fprintf(fid,'%s\n',str);
fclose(fid);

end
