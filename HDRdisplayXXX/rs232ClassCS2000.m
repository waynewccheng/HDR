%%
classdef rs232ClassCS2000
    %%
    properties
        S
    end
    
    %%
    methods

        function obj = rs232ClassCS2000 (portname)
            obj.S = serial(portname,'RequestToSend','off','BaudRate',38400,'DataBits',8,'StopBits',1,'FlowControl','none');
            fopen(obj.S);
        end
        
        %% close the serial port
        function close (obj)
            fclose(obj.S);
            delete(obj.S);
        end

        %% get a 13-terminating string from the serial port
        function ret = get (obj)
            flag = 1;
            ret = '';
            while flag
                b = fread(obj.S,1,'uint8');
                if b == 13
                    flag = 0;
                else
                    ret = [ret b];
                end
            end
        end

        %% send a string+13 to the serial port
        function send (obj,str)
            fwrite(obj.S,str);
            fwrite(obj.S,13);
        end

    end
end
