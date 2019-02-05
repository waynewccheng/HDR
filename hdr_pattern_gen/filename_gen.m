    function filename = filename_gen (lum)
        digit = 4;
        
        % lum=3.14 => "03p1400.hdr"
        part_integer = floor(lum);
        
        % uint16(10000 * 0.14)
        part_fraction = uint16((10.^digit)*(lum - part_integer));

        template = sprintf('%%02dp%%0%dd',digit);
        filename = sprintf(template,part_integer,part_fraction);
    end
    