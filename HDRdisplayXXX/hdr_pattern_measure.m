% Q: how to measure HDR luminance response automatically
% 1-28-2019, for SID 2019 submission
% copied from 10-bit
% hdr_pattern_measure(0.5:0.1:1,'mytest')

function hdr_pattern_measure (range,foldername)

% hdr_pattern(range)

load('hdr_pattern_gen_range','range')


cs = cs2000Class('COM7')

% https://docs.microsoft.com/en-us/windows-server/administration/windows-commands/wscript

h = actxserver('WScript.Shell');                      % ActiveX

%    cmd_line = sprintf('HDRDisplay -display 1 -hdr -fullscreen %s',pattern_fn)
cmd_line = sprintf('hdr_pattern_load')
h.Run(cmd_line);                                      % run it
h.AppActivate('HDRDisplay image viewer');             % brings HDR to focus

beep

%pause(60);

%
% wait for AntTweakBar to get ready for receiving key!!!
%
pause(5);

beep
beep

num_graylevel = numel(range);
num_repeat = 1;

clear luminance
luminance = zeros(num_graylevel,num_repeat);

k = 1;
for ddl = 1:num_graylevel
    
    for j = 1:num_repeat
        Yxy = cs.measure
        
        luminance(k,j) = Yxy(1);
    end
    
    %    h.SendKeys(sprintf('%c',27));                         % sends keystrokes
    h.SendKeys('v');                         % sends keystrokes
    
    k = k + 1;
end

cs.close

mkdir(foldername)

% copy the parameter files
copyfile('hdr_pattern_gen_range.mat',foldername)
copyfile('hdr_pattern_load.bat',foldername)

save([foldername '\luminance'],'luminance')

h.SendKeys(sprintf('%c',27));                         % sends keystrokes

plot(range, luminance, 'o-')
saveas(gcf,[foldername '\luminance'])

return

end
