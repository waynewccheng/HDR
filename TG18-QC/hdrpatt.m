% Generate HDR patterns for measuring

im = zeros(1000,1000,3);

k = 0;
for i = 0:9
    for j = 0:9
        lum = k*0.01;
        
        row = i * 100 + 1;
        col = j * 100 + 1;
        im(row:row+99,col:col+99,1) = lum;
        im(row:row+99,col:col+99,2) = lum;
        im(row:row+99,col:col+99,3) = lum;
        k = k + 1;
    end
end

imagesc(im)
hdrwrite(im,'myhdr.hdr');

