% Generate HDR patterns for measuring

patch_width = 100;
patch_n = 16;
im = uint8(zeros(patch_width*patch_n,patch_width*patch_n,3));

k = 0;
for i = 0:patch_n-1
    for j = 0:patch_n-1
        
        lum = k;
        
        row = i * patch_width + 1;
        col = j * patch_width + 1;
        im(row:row+patch_width-1,col:col+patch_width-1,1) = lum;
        im(row:row+patch_width-1,col:col+patch_width-1,2) = lum;
        im(row:row+patch_width-1,col:col+patch_width-1,3) = lum;
        k = k + 1;
    end
end

imagesc(im); colormap gray
imwrite(im,'ramp256.png');


iim = zeros(patch_width*patch_n,patch_width*patch_n,3);

k = 0;
for i = 0:patch_n-1
    for j = 0:patch_n-1
        
        lum = k/255*12.5;
        
        row = i * patch_width + 1;
        col = j * patch_width + 1;
        iim(row:row+patch_width-1,col:col+patch_width-1,1) = lum;
        iim(row:row+patch_width-1,col:col+patch_width-1,2) = lum;
        iim(row:row+patch_width-1,col:col+patch_width-1,3) = lum;
        k = k + 1;
    end
end

hdrwrite(iim,'ramp256x12.hdr');
