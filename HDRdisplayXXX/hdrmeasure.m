% Q: how to measure HDR luminance response automatically
% 1-28-2019, for SID 2019 submission
% copied from 10-bit

cs = cs2000Class('COM7')
clear luminance

ddl_range = [3]
k = 1;
% https://docs.microsoft.com/en-us/windows-server/administration/windows-commands/wscript

h = actxserver('WScript.Shell');                      % ActiveX

%    cmd_line = sprintf('HDRDisplay -display 1 -hdr -fullscreen %s',pattern_fn)
cmd_line = sprintf('showlum_1_12')
h.Run(cmd_line);                                      % run it
h.AppActivate('HDRDisplay image viewer');             % brings HDR to focus

%
% wait for AntTweakBar to get ready for receiving key!!!
%
pause(5);


for ddl = 1:31
    
    Yxy = cs.measure
    
    luminance(k) = Yxy(1)
    
    %    h.SendKeys(sprintf('%c',27));                         % sends keystrokes
    h.SendKeys('v');                         % sends keystrokes
    
    k = k + 1;
end

cs.close

save('luminance','luminance')

h.SendKeys(sprintf('%c',27));                         % sends keystrokes
plot(luminance,'-o')

return

% constant
filename = 'C:\cs2000go.txt';
no_steps = 18
gap_step = 60*(18/no_steps)

no_steps = 1024
gap_step = 1

% init
delete(filename)

luminance = zeros(no_steps);

cs = cs2000Class('COM8')

k = 1;
for ddl = 0:gap_step:1023
    % call hdr
    h = actxserver('WScript.Shell');
    cmd_line = sprintf('showgraylevel %d',ddl)
    h.Run(cmd_line);
    
    % wait for hdr
    hdr_ready = 0;
    while hdr_ready == 0
        fileID = fopen(filename,'r');
        if fileID == -1
            hdr_ready = 0;
        else
            hdr_ready = 1;
            fclose(fileID);
        end
    end
    
    % do my work
    Yxy = cs.measure
    
    luminance(k) = Yxy(1);
    
    % pause(10); %Waits for the application to load.
    
    % stop hdr
    h.AppActivate('HDRDisplay image viewer'); %Brings notepad to focus
    h.SendKeys(27); %Sends keystrokes
    
    % remove flag
    delete(filename);
    
    k = k + 1;
end

cs.close

save('luminance','luminance')
plot(luminance(:,1),'o')

