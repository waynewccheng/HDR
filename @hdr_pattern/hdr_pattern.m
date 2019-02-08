classdef hdr_pattern < handle
    
    properties
    end
    
    methods
        
        % 2-5-2019
        % rename to hdr_pattern_gen
        
        % 1-27-2019
        % Generate HDR patterns for measuring
        % gray patch on black (10% in size)
        % lum is the value for .HDR image [0,12.5]
        
        function obj = hdr_pattern (range)
            
%            range = 0.1234;
            save('hdr_pattern_gen_range','range');
            
            obj.hdr_pattern_gen_command_line('hdr_pattern_load.bat',range);
            
            for i = range
                obj.hdr_pattern_gen(i);
            end
            
        end
        
        function hdr_pattern_gen (obj, lum)
            
            % size of full screen 4K
            maxx = 3840;
            maxy = 2160;
            
            % center point
            centerx = maxx/2;
            centery = maxy/2;
            
            % area percentage
            perc = 0.1;
            
            % filename
            fn = obj.filename_gen(lum)
            
            % calculate sides
            % example: 10% area
            %  x * sqrt(0.1) * y * sqrt(0.1) = x * y * 0.1
            sizex = round(maxx * (perc.^0.5));
            sizey = round(maxy * (perc.^0.5));
            
            % make it an even number
            sizex = round(sizex / 2) * 2;
            sizey = round(sizey / 2) * 2;
            
            
            % canvas
            % black background to reduce glare
            im = zeros(maxy,maxx,3);
            
            % paint the white patch
            im(centery-sizey/2:centery+sizey/2,centerx-sizex/2:centerx+sizex/2,1) = lum;
            im(centery-sizey/2:centery+sizey/2,centerx-sizex/2:centerx+sizex/2,2) = lum;
            im(centery-sizey/2:centery+sizey/2,centerx-sizex/2:centerx+sizex/2,3) = lum;
            
            % visualize
            if 0
            imagesc(im); colormap gray;
            imwrite(im,[fn '.png']);
            end
            
            % create the HDR image file
            hdrwrite(im,[fn '.hdr']);
            
            return
            
            
        end
        
        function filename = filename_gen (obj, lum)
            digit = 4;
            
            % lum=3.14 => "03p1400.hdr"
            part_integer = floor(lum);
            
            % uint16(10000 * 0.14)
            part_fraction = uint16((10.^digit)*(lum - part_integer));
            
            template = sprintf('%%02dp%%0%dd',digit);
            filename = sprintf(template,part_integer,part_fraction);
        end
        
        
        % 2-5-2019
        % generate a .bat for running HDRDisplay
        % usage: hdr_pattern_gen_command_line('test.m',0.01:0.01:0.05)
        
        function hdr_pattern_gen_command_line (obj, fn_command, range)
            
            str = 'HDRDisplay -hdr -fullscreen';
            for i = range
                new_term = obj.filename_gen(i);
                str = sprintf('%s %s.hdr',str,new_term);
            end
            str
            
            fid = fopen(fn_command,'w');
            fprintf(fid,'%s\n',str);
            fclose(fid);
            
        end
        
    end
    
end

