% Generate HDR patterns for measuring

im = zeros(1000,1000,3);

for i = 4:5
    for j = 3:6
        row = i * 100 + 1;
        col = j * 100 + 1;
        im(row:row+99,col:col+99,1) = 12.5;
        im(row:row+99,col:col+99,2) = 12.5;
        im(row:row+99,col:col+99,3) = 12.5;
    end
end

imagesc(im)
hdrwrite(im,'myhdr_10.hdr');

