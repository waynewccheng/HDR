% Q: how to create an HDR image (.exr) for HDRDisplay
function dicom2hdr (fn)

fn_in = [fn '.dcm'];
fn_out = [fn '.hdr'];

X = double(dicomread(fn_in))./4096;

XX = X .^ 2.2;

XX3 = repmat(XX,1,1,3);

hdrwrite(XX3,fn_out)

end

