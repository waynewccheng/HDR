% Generate HDR patterns for measuring

im = zeros(1000,1000,3);

k = 0;
for i = 0:9
    for j = 0:9
        jnd = 1 - k*(1/255);
        lum = jnd .^ 2.2;
        
        row = i * 100 + 1;
        col = j * 100 + 1;
        im(row:row+99,col:col+99,1) = 1;
        im(row:row+99,col:col+99,2) = lum;
        im(row:row+99,col:col+99,3) = 1;
        k = k + 1;
    end
end

imagesc(im)
hdrwrite(im,'myhdr_per_255rc.hdr');

