import createSnLab from './snlab.mjs';
const M = await createSnLab();
const ver = M.ccall('snw_version','string',[],[]);
console.log('version', ver);
const ctx = M.ccall('snw_create','number',['number','number','number'],[11,52,1]);
if (!ctx) { console.error('no ctx'); process.exit(1); }
const a = M.ccall('snw_new_value','number',['number'],[ctx]);
const b = M.ccall('snw_new_value','number',['number'],[ctx]);
const c = M.ccall('snw_new_value','number',['number'],[ctx]);
let st = M.ccall('snw_set_f64','number',['number','number','number'],[ctx,a,0.1]);
st |= M.ccall('snw_set_f64','number',['number','number','number'],[ctx,b,1.0]);
// snw_binary(s, out, op, a, b) — op is string
const opPtr = M.stringToUTF8 ? null : null;
// use allocate via allocateUTF8 if available
let op;
if (typeof M.allocateUTF8 === 'function') op = M.allocateUTF8('add');
else {
  const n = M.lengthBytesUTF8('add')+1; op = M._malloc(n); M.stringToUTF8('add', op, n);
}
st = M.ccall('snw_binary','number',['number','number','string','number','number'],[ctx,c,'add',a,b]);
const err = M.ccall('snw_last_error','string',['number'],[ctx]);
const p = M.ccall('snw_to_str','number',['number','number','number'],[ctx,c,10]);
const s = p ? M.UTF8ToString(p) : '?';
const f = M.ccall('snw_to_f64','number',['number','number'],[ctx,c]);
console.log('0.1+1 ->', s, 'f64', f, 'st', st, 'err', err);
if (Math.abs(f - 1.1) > 1e-15) { console.error('bad sum'); process.exit(2); }
// div denorm-ish via large
const d = M.ccall('snw_new_value','number',['number'],[ctx]);
M.ccall('snw_set_f64','number',['number','number','number'],[ctx,a,2.0]);
M.ccall('snw_set_f64','number',['number','number','number'],[ctx,b,1.7976931348623157e+308]);
st = M.ccall('snw_binary','number',['number','number','string','number','number'],[ctx,d,'div',a,b]);
const fd = M.ccall('snw_to_f64','number',['number','number'],[ctx,d]);
const host = 2.0/1.7976931348623157e+308;
console.log('2/DBL_MAX sn', fd, 'host', host, 'eq', Object.is(fd, host) || fd===host);
if (fd !== host) { console.error('denorm div mismatch'); process.exit(3); }
// fma fused: (1+eps)^2 - 1
const fa = M.ccall('snw_new_value','number',['number'],[ctx]);
const fb = M.ccall('snw_new_value','number',['number'],[ctx]);
const fc = M.ccall('snw_new_value','number',['number'],[ctx]);
const fo = M.ccall('snw_new_value','number',['number'],[ctx]);
const eps = Math.pow(2, -52);
M.ccall('snw_set_f64','number',['number','number','number'],[ctx,fa,1+eps]);
M.ccall('snw_set_f64','number',['number','number','number'],[ctx,fb,1+eps]);
M.ccall('snw_set_f64','number',['number','number','number'],[ctx,fc,-1]);
st = M.ccall('snw_ternary','number',['number','number','string','number','number','number'],[ctx,fo,'fma',fa,fb,fc]);
const ff = M.ccall('snw_to_f64','number',['number','number'],[ctx,fo]);
const fexpect = 2*eps + eps*eps;
console.log('fma(1+eps,1+eps,-1)', ff, 'expect', fexpect, 'eq', Object.is(ff, fexpect));
if (!Object.is(ff, fexpect) && ff !== fexpect) { console.error('fma mismatch'); process.exit(4); }

if (p) M.ccall('snw_free_str','void',['number','number'],[ctx,p]);
M.ccall('snw_destroy','void',['number'],[ctx]);
console.log('web modular smoke OK');
