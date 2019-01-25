% Q: how to create an HDR image (.exr) for HDRDisplay
function dicom2png (fn)

fn_in = [fn '.dcm'];
fn_out = [fn '.png'];

X = double(dicomread(fn_in))./4096;

XX = uint8(X*255);

XX3 = repmat(XX,1,1,3);

imwrite(XX3,fn_out)

end

