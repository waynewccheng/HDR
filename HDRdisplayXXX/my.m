% Q: how to measure HDR luminance response automatically
% 1-28-2019, for SID 2019 submission
% copied from 10-bit

cs = cs2000Class('COM7')
clear luminance

k = 1;

% https://docs.microsoft.com/en-us/windows-server/administration/windows-commands/wscript
h = actxserver('WScript.Shell');                      % ActiveX

%    cmd_line = sprintf('HDRDisplay -display 1 -hdr -fullscreen %s',pattern_fn)
cmd_line = sprintf('showlum')
h.Run(cmd_line);                                      % run it
h.AppActivate('HDRDisplay image viewer');             % brings HDR to focus

%
% wait for HDRDisplay to load images!!!
%
pause(30);

%
% wait for AntTweakBar to get ready for receiving key!!!
%
pause(2);

for i = 1:41
    
    Yxy = cs.measure
    
    luminance(k) = Yxy(1)
    
    %    h.SendKeys(sprintf('%c',27));                         % sends keystrokes
    h.SendKeys('v');                         % sends keystrokes
    
    %
    % wait for AntTweakBar to get ready for receiving key!!!
    %
    pause(1);
    
    k = k + 1;
end

cs.close

save('luminance','luminance')

h.SendKeys(sprintf('%c',27));                         % sends keystrokes
plot(luminance,'-o')
