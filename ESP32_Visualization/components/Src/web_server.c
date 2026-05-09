#include "web_server.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "WEB_SERVER";
static httpd_handle_t server = NULL;

#define MAX_WS_CLIENTS 4
static int ws_fds[MAX_WS_CLIENTS];
static int ws_fd_count = 0;
static SemaphoreHandle_t ws_fds_mutex = NULL;

/* ------------------------------------------------------------------ */
/*  3D vizualizacija — čisti raw WebGL, bez CDN dependencija           */
/* ------------------------------------------------------------------ */
static const char *INDEX_HTML =
"<!DOCTYPE html><html><head>"
"<meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>Sound Localization 3D</title>"
"<style>"
"*{margin:0;padding:0;}body{overflow:hidden;background:#111;}"
"#hud{position:fixed;top:12px;left:12px;color:#fff;font:14px/1.6 monospace;"
"background:rgba(0,0,0,.65);padding:8px 14px;border-radius:6px;pointer-events:none;}"
"#sts{position:fixed;bottom:12px;left:12px;color:#888;font:12px monospace;}"
"</style></head><body>"
"<div id='hud'>Azimuth: --&deg; &nbsp; Strength: --</div>"
"<div id='sts'>Connecting...</div>"
"<script>"

/* --- WebGL canvas --- */
"var cv=document.createElement('canvas');"
"document.body.appendChild(cv);"
"var gl=cv.getContext('webgl');"
"function rsz(){cv.width=innerWidth;cv.height=innerHeight;gl.viewport(0,0,cv.width,cv.height);}"
"rsz();window.addEventListener('resize',rsz);"

/* --- Shaders --- */
"var VS='attribute vec3 aPos;attribute vec3 aNrm;'"
"+'uniform mat4 uM,uMVP;varying vec3 vN;'"
"+'void main(){vN=normalize(mat3(uM)*aNrm);gl_Position=uMVP*vec4(aPos,1.);}'; "
"var FS='precision mediump float;uniform vec3 uCol;uniform int uLit;varying vec3 vN;'"
"+'void main(){if(uLit<1){gl_FragColor=vec4(uCol,1.);return;}'"
"+'vec3 L=normalize(vec3(.55,1.,.35));float d=max(dot(normalize(vN),L),0.);'"
"+'gl_FragColor=vec4(uCol*(.3+.7*d),1.);}'; "
"function mkSh(t,s){var sh=gl.createShader(t);gl.shaderSource(sh,s);gl.compileShader(sh);return sh;}"
"var prg=gl.createProgram();"
"gl.attachShader(prg,mkSh(gl.VERTEX_SHADER,VS));"
"gl.attachShader(prg,mkSh(gl.FRAGMENT_SHADER,FS));"
"gl.linkProgram(prg);gl.useProgram(prg);"
"var aP=gl.getAttribLocation(prg,'aPos'),aN=gl.getAttribLocation(prg,'aNrm');"
"var uM_=gl.getUniformLocation(prg,'uM'),uMVP_=gl.getUniformLocation(prg,'uMVP');"
"var uC_=gl.getUniformLocation(prg,'uCol'),uL_=gl.getUniformLocation(prg,'uLit');"

/* --- Matrix math (column-major, WebGL convention) --- */
"function mm(a,b){"
"var c=new Float32Array(16);"
"for(var i=0;i<4;i++)for(var j=0;j<4;j++)for(var k=0;k<4;k++)c[j*4+i]+=a[k*4+i]*b[j*4+k];"
"return c;}"
"function pm(fov,asp,n,f){"
"var t=Math.tan(fov*Math.PI/360),m=new Float32Array(16);"
"m[0]=1/(t*asp);m[5]=1/t;m[10]=(f+n)/(n-f);m[11]=-1;m[14]=2*f*n/(n-f);return m;}"
"function lm(ex,ey,ez){"
"var fx=-ex,fy=-ey,fz=-ez,l=Math.sqrt(fx*fx+fy*fy+fz*fz);"
"fx/=l;fy/=l;fz/=l;"
"var rx=-fz,rz=fx,l2=Math.sqrt(rx*rx+rz*rz);rx/=l2;rz/=l2;"
"var ux=-rz*fy,uy=rz*fx-rx*fz,uz=rx*fy;"
"var m=new Float32Array(16);"
"m[0]=rx;m[1]=ux;m[2]=-fx;"
"m[4]=0; m[5]=uy;m[6]=-fy;"
"m[8]=rz;m[9]=uz;m[10]=-fz;"
"m[11]=0;m[15]=1;"
"m[12]=-(rx*ex+rz*ez);m[13]=-(ux*ex+uy*ey+uz*ez);m[14]=fx*ex+fy*ey+fz*ez;"
"return m;}"
"function tm(x,y,z){return new Float32Array([1,0,0,0,0,1,0,0,0,0,1,0,x,y,z,1]);}"
"function sm(s){return new Float32Array([s,0,0,0,0,s,0,0,0,0,s,0,0,0,0,1]);}"
"var ID=new Float32Array([1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]);"

/* --- Geometry: box (face normals, 36 vertices) --- */
"function boxGeo(){"
"var v=[];"
"var F=["
"[0,0,1, -1,-1,1, 1,-1,1, 1,1,1, -1,1,1],"
"[0,0,-1, 1,-1,-1,-1,-1,-1,-1,1,-1, 1,1,-1],"
"[1,0,0, 1,-1,-1, 1,-1,1, 1,1,1, 1,1,-1],"
"[-1,0,0,-1,-1,1,-1,-1,-1,-1,1,-1,-1,1,1],"
"[0,1,0, -1,1,1, 1,1,1, 1,1,-1,-1,1,-1],"
"[0,-1,0,-1,-1,-1, 1,-1,-1, 1,-1,1,-1,-1,1]];"
"F.forEach(function(f){"
"var n=[f[0],f[1],f[2]];"
"var q=[[f[3],f[4],f[5]],[f[6],f[7],f[8]],[f[9],f[10],f[11]],[f[12],f[13],f[14]]];"
"[[0,1,2],[0,2,3]].forEach(function(t){"
"t.forEach(function(i){q[i].forEach(function(x){v.push(x);});n.forEach(function(x){v.push(x);});});});"
"});return new Float32Array(v);}"

/* --- Geometry: UV sphere (pos=norm on unit sphere) --- */
"function sphGeo(la,lo){"
"var v=[];"
"for(var i=0;i<la;i++){"
"var t0=(i/la-.5)*Math.PI,t1=((i+1)/la-.5)*Math.PI;"
"for(var j=0;j<lo;j++){"
"var p0=j/lo*2*Math.PI,p1=(j+1)/lo*2*Math.PI;"
"var pt=function(t,p){return[Math.cos(t)*Math.cos(p),Math.sin(t),Math.cos(t)*Math.sin(p)];};"
"var a=pt(t0,p0),b=pt(t1,p0),c=pt(t1,p1),d=pt(t0,p1);"
"[a,b,c,a,c,d].forEach(function(x){"
"x.forEach(function(n){v.push(n);});x.forEach(function(n){v.push(n);});});}}"
"return new Float32Array(v);}"

/* --- Geometry: front-hemisphere arc (-90 do +90) i grid (6 floats/vert: pos+dummy norm) --- */
"function arcGeo(r,segs){"
"var v=[];"
"for(var i=0;i<segs;i++){"
"var a0=(i/segs-0.5)*Math.PI,a1=((i+1)/segs-0.5)*Math.PI;"
"v.push(Math.sin(a0)*r,0,-Math.cos(a0)*r,0,1,0);"
"v.push(Math.sin(a1)*r,0,-Math.cos(a1)*r,0,1,0);}"
"return new Float32Array(v);}"
"function gridGeo(sz,d){"
"var v=[],step=sz*2/d;"
"for(var i=0;i<=d;i++){"
"var x=-sz+i*step;"
"v.push(x,0,-sz,0,1,0,x,0,sz,0,1,0);"
"v.push(-sz,0,x,0,1,0,sz,0,x,0,1,0);}"
"return new Float32Array(v);}"

/* --- GPU buffers --- */
"function mkBuf(data){"
"var b=gl.createBuffer();"
"gl.bindBuffer(gl.ARRAY_BUFFER,b);"
"gl.bufferData(gl.ARRAY_BUFFER,data,gl.STATIC_DRAW);"
"return{b:b,n:data.length/6};}"
"var bBox=mkBuf(boxGeo()),bSph=mkBuf(sphGeo(12,16));"
"var bArc=mkBuf(arcGeo(8,48)),bGrid=mkBuf(gridGeo(12,24));"

/* --- Draw call --- */
"var VP=ID;"
"function drw(buf,mode,M,col,lit){"
"gl.bindBuffer(gl.ARRAY_BUFFER,buf.b);"
"gl.enableVertexAttribArray(aP);gl.enableVertexAttribArray(aN);"
"gl.vertexAttribPointer(aP,3,gl.FLOAT,false,24,0);"
"gl.vertexAttribPointer(aN,3,gl.FLOAT,false,24,12);"
"gl.uniformMatrix4fv(uM_,false,M);"
"gl.uniformMatrix4fv(uMVP_,false,mm(VP,M));"
"gl.uniform3fv(uC_,new Float32Array(col));"
"gl.uniform1i(uL_,lit?1:0);"
"gl.drawArrays(mode,0,buf.n);}"

/* --- Orbit camera (mouse + touch + wheel) --- */
"var cAz=0.4,cEl=0.7,cDst=22,drag=false,lx=0,ly=0;"
"cv.addEventListener('mousedown',function(e){drag=true;lx=e.clientX;ly=e.clientY;});"
"document.addEventListener('mouseup',function(){drag=false;});"
"cv.addEventListener('mousemove',function(e){"
"if(!drag)return;"
"cAz-=(e.clientX-lx)*.009;"
"cEl=Math.max(.05,Math.min(1.4,cEl+(e.clientY-ly)*.009));"
"lx=e.clientX;ly=e.clientY;});"
"cv.addEventListener('wheel',function(e){"
"cDst=Math.max(8,Math.min(50,cDst+e.deltaY*.04));e.preventDefault();},{passive:false});"
"cv.addEventListener('touchstart',function(e){drag=true;lx=e.touches[0].clientX;ly=e.touches[0].clientY;e.preventDefault();},{passive:false});"
"cv.addEventListener('touchend',function(){drag=false;});"
"cv.addEventListener('touchmove',function(e){"
"if(!drag)return;"
"cAz-=(e.touches[0].clientX-lx)*.009;"
"cEl=Math.max(.05,Math.min(1.4,cEl+(e.touches[0].clientY-ly)*.009));"
"lx=e.touches[0].clientX;ly=e.touches[0].clientY;e.preventDefault();},{passive:false});"

/* --- Sound sphere pool (max 20, fade 5s) --- */
"var pool=[];"
"function addS(az,po){"
"var a=az*Math.PI/180,p=po*Math.PI/180;"
"pool.push({x:8*Math.sin(a)*Math.cos(p),y:8*Math.sin(p),z:-8*Math.cos(a)*Math.cos(p),t:Date.now()});"
"while(pool.length>20)pool.shift();}"

/* --- WebSocket --- */
"var hud=document.getElementById('hud'),sts=document.getElementById('sts');"
"var ws=new WebSocket('ws://'+location.host+'/ws');"
"ws.onopen=function(){sts.textContent='Connected';sts.style.color='#0f0';};"
"ws.onclose=function(){sts.textContent='Disconnected';sts.style.color='#f44';};"
"ws.onmessage=function(ev){"
"try{var d=JSON.parse(ev.data);"
"hud.innerHTML='Azimuth: '+d.azimuth.toFixed(1)+'&deg; &nbsp; Strength: '+d.strength;"
"addS(d.azimuth,d.polar||0);}catch(e){}};"

/* --- Render loop --- */
"gl.enable(gl.DEPTH_TEST);"
"function render(){"
"requestAnimationFrame(render);"
"gl.clearColor(.067,.067,.067,1);"
"gl.clear(gl.COLOR_BUFFER_BIT|gl.DEPTH_BUFFER_BIT);"
"var ex=cDst*Math.cos(cEl)*Math.sin(cAz);"
"var ey=cDst*Math.sin(cEl);"
"var ez=cDst*Math.cos(cEl)*Math.cos(cAz);"
"VP=mm(pm(60,cv.width/cv.height,.1,200),lm(ex,ey,ez));"
"drw(bGrid,gl.LINES,ID,[.2,.2,.2],false);"
"drw(bArc,gl.LINES,ID,[.27,.35,.42],false);"
"drw(bBox,gl.TRIANGLES,ID,[0,.67,1.],true);"
"var now=Date.now();"
"for(var i=pool.length-1;i>=0;i--){"
"var age=now-pool[i].t;"
"if(age>5000){pool.splice(i,1);continue;}"
"var f=1-age/5000;"
"drw(bSph,gl.TRIANGLES,mm(tm(pool[i].x,pool[i].y,pool[i].z),sm(.45)),[f,.13*f,0],true);}}"
"render();"
"</script></body></html>";

/* ------------------------------------------------------------------ */
/*  HTTP GET /                                                          */
/* ------------------------------------------------------------------ */
static esp_err_t root_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
/*  WebSocket handler                                                   */
/* ------------------------------------------------------------------ */
static esp_err_t ws_handler(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        int fd = httpd_req_to_sockfd(req);
        xSemaphoreTake(ws_fds_mutex, portMAX_DELAY);
        /* Provjeri je li fd već u listi (npr. browser refresh) */
        int already = 0;
        for (int i = 0; i < ws_fd_count; i++) {
            if (ws_fds[i] == fd) { already = 1; break; }
        }
        if (!already && ws_fd_count < MAX_WS_CLIENTS) {
            ws_fds[ws_fd_count++] = fd;
            ESP_LOGI(TAG, "WS klijent spojen, fd=%d (count=%d)", fd, ws_fd_count);
        }
        xSemaphoreGive(ws_fds_mutex);
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(ws_pkt));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) return ret;

    if (ws_pkt.len) {
        uint8_t *buf = calloc(1, ws_pkt.len + 1);
        ws_pkt.payload = buf;
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        free(buf);
    }
    return ret;
}

/* ------------------------------------------------------------------ */
/*  Slanje podataka svim WS klijentima                                 */
/* ------------------------------------------------------------------ */
void web_server_send_data(float azimuth, float polar, uint8_t strength) {
    if (!server || !ws_fds_mutex) return;

    char json[80];
    snprintf(json, sizeof(json),
             "{\"azimuth\":%.1f,\"polar\":%.1f,\"strength\":%u}",
             azimuth, polar, strength);

    httpd_ws_frame_t ws_pkt = {
        .final      = true,
        .fragmented = false,
        .type       = HTTPD_WS_TYPE_TEXT,
        .payload    = (uint8_t *)json,
        .len        = strlen(json),
    };

    /* Snapshot fd liste pod lockom — minimiziramo trajanje locka da ne blokiramo
     * ws_handler dok šaljemo (httpd_ws_send_frame_async može potrajati). */
    int local_fds[MAX_WS_CLIENTS];
    int local_count = 0;
    xSemaphoreTake(ws_fds_mutex, portMAX_DELAY);
    local_count = ws_fd_count;
    memcpy(local_fds, ws_fds, sizeof(int) * local_count);
    xSemaphoreGive(ws_fds_mutex);

    if (local_count == 0) return;

    /* Pošalji na svaki fd; sakupi koje treba ukloniti */
    int dead_fds[MAX_WS_CLIENTS];
    int dead_count = 0;
    for (int i = 0; i < local_count; i++) {
        esp_err_t err = httpd_ws_send_frame_async(server, local_fds[i], &ws_pkt);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Greška pri slanju na fd=%d (err=0x%x), uklanjam", local_fds[i], err);
            dead_fds[dead_count++] = local_fds[i];
        }
    }

    /* Ukloni mrtve fd-ove pod lockom */
    if (dead_count > 0) {
        xSemaphoreTake(ws_fds_mutex, portMAX_DELAY);
        for (int d = 0; d < dead_count; d++) {
            for (int i = 0; i < ws_fd_count; i++) {
                if (ws_fds[i] == dead_fds[d]) {
                    ws_fds[i] = ws_fds[--ws_fd_count];
                    break;
                }
            }
        }
        xSemaphoreGive(ws_fds_mutex);
    }
}

/* ------------------------------------------------------------------ */
/*  Inicijalizacija servera                                             */
/* ------------------------------------------------------------------ */
void web_server_init(void) {
    if (!ws_fds_mutex) {
        ws_fds_mutex = xSemaphoreCreateMutex();
        if (!ws_fds_mutex) {
            ESP_LOGE(TAG, "Neuspjelo kreiranje ws_fds_mutex-a");
            return;
        }
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;

    static const httpd_uri_t root_uri = {
        .uri      = "/",
        .method   = HTTP_GET,
        .handler  = root_get_handler,
        .user_ctx = NULL,
    };

    static const httpd_uri_t ws_uri = {
        .uri          = "/ws",
        .method       = HTTP_GET,
        .handler      = ws_handler,
        .user_ctx     = NULL,
        .is_websocket = true,
    };

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &root_uri);
        httpd_register_uri_handler(server, &ws_uri);
        ESP_LOGI(TAG, "HTTP server pokrenut na portu 80");
    } else {
        ESP_LOGE(TAG, "Greška pri pokretanju HTTP servera");
    }
}
