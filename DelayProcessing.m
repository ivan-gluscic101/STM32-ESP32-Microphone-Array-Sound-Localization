% =========================================================================
%  TDOA 3D lokalizacija zvuka - tetrahedral niz mikrofona
%  STM32G474RE - 4x ADC @ 64 kHz, USART3 binarni tok
%
%  Geometrija: pravilni tetraedar, brid = 10 cm
%    MIC1: (0,     0,      0     )
%    MIC2: (0.1,   0,      0     )
%    MIC3: (0.05,  0.0866, 0     )
%    MIC4: (0.05,  0.0289, 0.0816)
%
%  Metoda: GCC-PHAT xcorr za TDOA, zatim A\(c*tau) za 3D smjer
% =========================================================================

clear; clc; close all;

%% -------------------------------------------------------------------------
%  KONFIGURACIJA
% -------------------------------------------------------------------------
COM_PORT       = 'COM5';
BAUD_RATE      = 115200;
NUM_CHANNELS   = 4;
SAMPLES_PER_CH = 1024;
NUM_FRAMES     = 4;          % 4096 uzoraka/kanalu = 64 ms
FS             = 64000;      % Hz

C_SOUND        = 343.0;      % m/s
HPF_CUTOFF_HZ  = 300;        % Hz

% Pozicije mikrofona [m] - pravilni tetraedar, brid 10 cm
EDGE = 0.10;
MIC_POS = [
    0,          0,                       0;
    EDGE,       0,                       0;
    EDGE*0.5,   EDGE*0.86602540378,      0;
    EDGE*0.5,   EDGE*0.28867513459,      EDGE*0.81649658092
];

MAX_TDOA_US = EDGE / C_SOUND * 1e6;
MAX_LAG     = ceil(MAX_TDOA_US * 1e-6 * FS * 1.5);

% Detekcija zvučnog događaja (pljeskanje)
CLAP_THRESHOLD_DB = 20;     % dB iznad noise floora
PRE_EVENT_MS      = 8;      % ms prije vrha energije
POST_EVENT_MS     = 20;     % ms nakon vrha energije

% Detekcija i uklanjanje kratkih prekida napajanja (glitch)
% Glitch = nagla promjena amplitude za > GLITCH_DIFF_FACTOR * median_diff
% u trajanju od <= GLITCH_MAX_SAMPLES uzoraka
GLITCH_DIFF_FACTOR = 8;     % faktor iznad medijana apsolutnog diffa
GLITCH_MAX_SAMPLES = 3;     % maksimalna duljina glitcha koji se interpolira

bytes_per_frame = NUM_CHANNELS * SAMPLES_PER_CH * 2;
samples_total   = SAMPLES_PER_CH * NUM_FRAMES;

%% -------------------------------------------------------------------------
%  SERIJSKO ČITANJE
% -------------------------------------------------------------------------
fprintf('Otvaranje %s @ %d baud...\n', COM_PORT, BAUD_RATE);
s = serialport(COM_PORT, BAUD_RATE, 'Timeout', 15);
flush(s);

fprintf('Trazenje sinkronizacijske zaglavlje...\n');
win = uint8([0 0 0 0]);
found = false;
for k = 1:(2*bytes_per_frame + 16)
    b   = read(s, 1, 'uint8');
    win = [win(2:4), uint8(b)];
    if all(win == 255), found = true; break; end
end
if ~found, clear s; error('Sync header nije pronađen.'); end

fprintf('Sync OK. Citanje %d okvira...\n', NUM_FRAMES);
raw_all = uint8(zeros(bytes_per_frame * NUM_FRAMES, 1));
raw_all(1:bytes_per_frame) = read(s, bytes_per_frame, 'uint8');
for f = 2:NUM_FRAMES
    win2 = uint8([0 0 0 0]);
    for k = 1:(bytes_per_frame + 16)
        b2   = read(s, 1, 'uint8');
        win2 = [win2(2:4), uint8(b2)];
        if all(win2 == 255), break; end
    end
    raw_all((f-1)*bytes_per_frame+1 : f*bytes_per_frame) = read(s, bytes_per_frame, 'uint8');
end
clear s;

%% -------------------------------------------------------------------------
%  PARSIRANJE
% -------------------------------------------------------------------------
ch = cell(1, NUM_CHANNELS);
for i = 1:NUM_CHANNELS, ch{i} = zeros(samples_total, 1); end
for f = 1:NUM_FRAMES
    seg  = raw_all((f-1)*bytes_per_frame+1 : f*bytes_per_frame);
    u16  = uint16(seg(1:2:end)) + uint16(seg(2:2:end)) * 256;
    mat  = reshape(u16, NUM_CHANNELS, SAMPLES_PER_CH)';
    idx0 = (f-1)*SAMPLES_PER_CH;
    for i = 1:NUM_CHANNELS
        ch{i}(idx0+1 : idx0+SAMPLES_PER_CH) = double(mat(:,i));
    end
end
fprintf('Parsirano: %d uzoraka/kanalu (%.1f ms).\n', samples_total, samples_total/FS*1e3);

%% -------------------------------------------------------------------------
%  DETEKCIJA I UKLANJANJE KRATKIH PREKIDA NAPAJANJA (GLITCH)
%
%  Kratki prekid napajanja izgleda ovako u sirovalim podacima:
%    ... 3100  3105  3102  [12]  [18]  3098  3103 ...
%                          ^^^^^^^^^^^^^
%                          1-2 uzorka gdje ADC čita krivi napon
%
%  Algoritam:
%    1. Izračunaj apsolutnu razliku između susjednih uzoraka: d = |diff(x)|
%    2. Prag: GLITCH_DIFF_FACTOR * median(d)  [adaptivno po kanalu]
%    3. Pronađi mjesta gdje d > prag (ulaz i izlaz glitcha)
%    4. Ako su dva uzastopna prevelika skoka na udaljenosti <= GLITCH_MAX_SAMPLES
%       -> taj segment je glitch -> linearna interpolacija između rubova
%
%  Glitchevi koji traju dulje od GLITCH_MAX_SAMPLES se NE diraju
%  (mogli bi biti pravi akustički tranzijent).
% -------------------------------------------------------------------------
ch_raw = ch;   % čuvamo sirove podatke za Graf 1 usporedbu
glitch_mask = cell(1, NUM_CHANNELS);  % za iscrtavanje na grafu

fprintf('\n--- Detekcija glitch uzoraka ---\n');
for i = 1:NUM_CHANNELS
    [ch{i}, glitch_mask{i}, n_fixed] = remove_glitches(ch{i}, ...
        GLITCH_DIFF_FACTOR, GLITCH_MAX_SAMPLES);
    if n_fixed > 0
        fprintf('  MIC%d: popravljeno %d glitch segmenata\n', i, n_fixed);
    else
        fprintf('  MIC%d: nema glitcheva\n', i);
    end
end

%% -------------------------------------------------------------------------
%  HIGH-PASS FILTRIRANJE
% -------------------------------------------------------------------------
[b_h, a_h] = butter(2, HPF_CUTOFF_HZ/(FS/2), 'high');
ch_f = cellfun(@(x) filtfilt(b_h, a_h, x), ch, 'uni', 0);

%% -------------------------------------------------------------------------
%  DETEKCIJA ZVUČNOG DOGAĐAJA (STE energija)
% -------------------------------------------------------------------------
ste_win  = round(2e-3 * FS);
ste_hop  = round(ste_win / 2);
n_ste    = floor((samples_total - ste_win) / ste_hop) + 1;
ste_lin  = zeros(1, n_ste);
for k = 1:n_ste
    idx = (k-1)*ste_hop + (1:ste_win);
    e = 0;
    for i = 1:NUM_CHANNELS, e = e + sum(ch_f{i}(idx).^2); end
    ste_lin(k) = e / (ste_win * NUM_CHANNELS);
end
ste_dB     = 10 * log10(ste_lin + 1e-10);
nf_dB      = prctile(ste_dB, 20);
thr_dB     = nf_dB + CLAP_THRESHOLD_DB;
t_ste_ms   = ((0:n_ste-1)*ste_hop + ste_win/2) / FS * 1e3;

[~, pk_ste]  = max(ste_lin);
peak_sample  = round((pk_ste-1)*ste_hop + ste_win/2);
win_start    = max(1, peak_sample - round(PRE_EVENT_MS  * 1e-3 * FS));
win_end      = min(samples_total, peak_sample + round(POST_EVENT_MS * 1e-3 * FS));
ch_win       = cellfun(@(x) x(win_start:win_end), ch_f, 'uni', 0);

event_detected = max(ste_dB) > thr_dB;
if event_detected
    fprintf('\n[DETEKTIRAN] Zvucni dogadaj @ %.1f ms, xcorr prozor: %.1f-%.1f ms\n', ...
            peak_sample/FS*1e3, win_start/FS*1e3, win_end/FS*1e3);
else
    fprintf('\n[UPOZORENJE] Nije detektiran zvuk iznad praga (%+.0f dB).\n', CLAP_THRESHOLD_DB);
    fprintf('  Povecaj glasnocu ili smanji CLAP_THRESHOLD_DB.\n');
end

%% -------------------------------------------------------------------------
%  PROVJERA KVALITETE SIGNALA
% -------------------------------------------------------------------------
fprintf('\n--- Kvaliteta signala ---\n');
for i = 1:NUM_CHANNELS
    fprintf('  MIC%d: DC=%.0f  vr-vr=%4.0f  RMS(fil)=%5.1f\n', ...
            i, mean(ch{i}), max(ch{i})-min(ch{i}), rms(ch_f{i}));
end

%% -------------------------------------------------------------------------
%  GCC-PHAT TDOA MJERENJE
% -------------------------------------------------------------------------
tdoa_samples = zeros(1, NUM_CHANNELS-1);
tdoa_us      = zeros(1, NUM_CHANNELS-1);
tdoa_peak    = zeros(1, NUM_CHANNELS-1);

fprintf('\n--- GCC-PHAT TDOA (ref: MIC1, max lag: +/-%d uzoraka) ---\n', MAX_LAG);
for i = 2:NUM_CHANNELS
    [xc, lags]        = gcc_phat(ch_win{1}, ch_win{i}, MAX_LAG);
    [pk, idx]         = max(xc);
    tdoa_samples(i-1) = lags(idx);
    tdoa_us(i-1)      = lags(idx) / FS * 1e6;
    tdoa_peak(i-1)    = pk;
    fprintf('  MIC1-MIC%d: %+d uzoraka | %+.2f us | vrh=%.3f\n', ...
            i, tdoa_samples(i-1), tdoa_us(i-1), pk);
end

%% -------------------------------------------------------------------------
%  3D DOA PROCJENA
% -------------------------------------------------------------------------
A_mat = MIC_POS(1,:) - MIC_POS(2:end,:);
tau_s = (tdoa_us * 1e-6)';
u_est = A_mat \ (C_SOUND * tau_s);
u_n   = norm(u_est);
if u_n < 0.01
    warning('Vektor smjera je gotovo nula.');
    u_unit = [0; 0; 1];
else
    u_unit = max(-1, min(1, u_est / u_n));
end
azimuth_deg   = atan2d(u_unit(2), u_unit(1));
elevation_deg = asind(u_unit(3));

fprintf('\n=> Smjer dolaska zvuka: Azimut=%+.1f deg  Elevacija=%+.1f deg\n', ...
        azimuth_deg, elevation_deg);

%% =========================================================================
%  GRAFOVI
% =========================================================================
t_ms   = (0:samples_total-1) / FS * 1e3;
colors = lines(NUM_CHANNELS);

%--------------------------------------------------------------------------
%  FIGURA 1: Pregled signala
%
%  [1] Sirovi signali PRIJE korekcije
%      Glitch uzorci su označeni crvenim križićima - provjeri da algoritam
%      ispravno pronalazi kratke prekide napajanja.
%      Ako su označeni pravi tranzijenti (pljeskanje) -> povećaj GLITCH_MAX_SAMPLES
%      ili GLITCH_DIFF_FACTOR da ih ne briše.
%
%  [2] Signali NAKON korekcije glitcheva (ali još bez HPF)
%      Interpolirani segmenti bi trebali glatko premostiti prekid.
%
%  [3] STE energija s pragom i xcorr prozorom
%
%  [4] Zoom xcorr prozora (filtrirano) - traži vremenski pomak
%--------------------------------------------------------------------------
figure('Name','Fig 1 - Pregled signala','Color','w','Position',[20 20 1050 950]);

subplot(4,1,1); hold on; grid on;
for i = 1:NUM_CHANNELS
    plot(t_ms, ch_raw{i}, 'Color', colors(i,:));
end
for i = 1:NUM_CHANNELS
    gm = glitch_mask{i};
    if any(gm)
        plot(t_ms(gm), ch_raw{i}(gm), 'rx', 'MarkerSize', 8, 'LineWidth', 2);
    end
end
title('Sirovi signali PRIJE korekcije — crveni x = detektirani glitch uzorci');
xlabel('Vrijeme [ms]'); ylabel('ADC koraci');
lh = legend(arrayfun(@(i)sprintf('MIC%d',i),1:NUM_CHANNELS,'uni',0),'Location','northeast');
if any(cellfun(@any, glitch_mask))
    legend([lh.String, {'Glitch'}], 'Location','northeast');
end

subplot(4,1,2); hold on; grid on;
for i = 1:NUM_CHANNELS
    plot(t_ms, ch{i}, 'Color', colors(i,:));
end
title(sprintf('Signali NAKON korekcije glitcheva (prije HPF filtra)'));
xlabel('Vrijeme [ms]'); ylabel('ADC koraci');
legend(arrayfun(@(i)sprintf('MIC%d',i),1:NUM_CHANNELS,'uni',0),'Location','northeast');

subplot(4,1,3); hold on; grid on;
plot(t_ste_ms, ste_dB, 'b-', 'LineWidth',1.2, 'DisplayName','STE energija');
yline(nf_dB,  'g--', 'LineWidth',1.2, 'DisplayName','Noise floor (20. pct)');
yline(thr_dB, 'r-',  'LineWidth',1.5, 'DisplayName',sprintf('Prag (+%d dB)',CLAP_THRESHOLD_DB));
yl = ylim;
patch([win_start/FS*1e3, win_end/FS*1e3, win_end/FS*1e3, win_start/FS*1e3], ...
      [yl(1), yl(1), yl(2), yl(2)], [1 0.9 0.3], ...
      'FaceAlpha',0.25, 'EdgeColor','none', 'DisplayName','GCC-PHAT prozor');
xline(peak_sample/FS*1e3, 'm--', 'LineWidth',1.2, 'DisplayName','Vrh energije');
if event_detected
    title(sprintf('STE energija — DETEKTIRAN zvuk @ %.1f ms', peak_sample/FS*1e3));
else
    title('STE energija — NIJE detektiran zvuk (provjeri CLAP_THRESHOLD_DB)');
end
xlabel('Vrijeme [ms]'); ylabel('Energija [dB]');
legend('Location','northeast'); xlim([0 samples_total/FS*1e3]);

subplot(4,1,4); hold on; grid on;
t_win_us = (0:numel(ch_win{1})-1) / FS * 1e6;
for i = 1:NUM_CHANNELS
    plot(t_win_us, ch_win{i}, '-o', 'Color', colors(i,:), 'MarkerSize', 2);
end
title('Zoom: xcorr prozor (filtrirano) — traži pomak između krivulja');
xlabel('Vrijeme [\mus]'); ylabel('ADC koraci');
legend(arrayfun(@(i)sprintf('MIC%d',i),1:NUM_CHANNELS,'uni',0),'Location','northeast');

%--------------------------------------------------------------------------
%  FIGURA 2: GCC-PHAT TDOA analiza
%--------------------------------------------------------------------------
figure('Name','Fig 2 - GCC-PHAT TDOA analiza','Color','w','Position',[20 1020 1050 560]);

for i = 2:NUM_CHANNELS
    subplot(2, 2, i-1); hold on; grid on;
    [xc, lags_s] = gcc_phat(ch_win{1}, ch_win{i}, MAX_LAG);
    plot(lags_s/FS*1e6, xc, 'b-', 'LineWidth',1.2);
    xline(tdoa_us(i-1), 'r--', 'LineWidth',1.5);
    plot(tdoa_us(i-1), tdoa_peak(i-1), 'rv', 'MarkerSize',9, 'MarkerFaceColor','r');
    xlim([-MAX_LAG MAX_LAG]/FS*1e6);
    title(sprintf('MIC1 vs MIC%d:  %+d uzoraka  (%+.1f \\mus)', ...
          i, tdoa_samples(i-1), tdoa_us(i-1)));
    xlabel('Pomak [\mus]'); ylabel('GCC-PHAT');
    text(0.02,0.93,sprintf('vrh=%.3f',tdoa_peak(i-1)), ...
         'Units','normalized','FontSize',8,'Color','r','FontWeight','bold');
end

subplot(2,2,4); hold on; grid on;
b_bar = bar(1:NUM_CHANNELS-1, tdoa_us, 0.5, 'FaceColor','flat');
for i = 1:NUM_CHANNELS-1, b_bar.CData(i,:) = colors(i+1,:); end
yline(0,'k--'); yline(MAX_TDOA_US,'r:','LineWidth',1.5); yline(-MAX_TDOA_US,'r:','LineWidth',1.5);
ylim([-MAX_TDOA_US*1.4 MAX_TDOA_US*1.4]);
for i = 1:NUM_CHANNELS-1
    off = sign(tdoa_us(i)+0.01) * MAX_TDOA_US * 0.18;
    text(i, tdoa_us(i)+off, sprintf('%.1f \\mus',tdoa_us(i)), ...
         'HorizontalAlignment','center','FontSize',9,'FontWeight','bold');
end
set(gca,'XTick',1:NUM_CHANNELS-1, ...
    'XTickLabel',arrayfun(@(i)sprintf('MIC1 vs MIC%d',i+1),1:NUM_CHANNELS-1,'uni',0));
title('TDOA sažetak relativno na MIC1'); ylabel('Kašnjenje [\mus]');
text(0.5, MAX_TDOA_US*1.1, sprintf('+/- %.0f \\mus (max)',MAX_TDOA_US),'Color','r','FontSize',8);

%--------------------------------------------------------------------------
%  FIGURA 3: 3D smjer dolaska zvuka
%--------------------------------------------------------------------------
figure('Name','Fig 3 - 3D smjer dolaska zvuka','Color','w','Position',[1090 20 560 560]);
hold on; grid on; axis equal;

[xs,ys,zs] = sphere(40);
surf(xs,ys,zs,'FaceAlpha',0.07,'EdgeAlpha',0.04,'FaceColor',[0.7 0.8 1]);
quiver3(0,0,0,1.25,0,0,0,'k-','LineWidth',1.2); text(1.35,0,0,'X','FontSize',9);
quiver3(0,0,0,0,1.25,0,0,'k-','LineWidth',1.2); text(0,1.35,0,'Y','FontSize',9);
quiver3(0,0,0,0,0,1.25,0,'k-','LineWidth',1.2); text(0,0,1.35,'Z','FontSize',9);

mic_sc = 0.12 / (EDGE*0.5);
mic_c  = mean(MIC_POS,1);
for i = 1:NUM_CHANNELS
    mp = (MIC_POS(i,:) - mic_c) * mic_sc;
    plot3(mp(1),mp(2),mp(3),'s','Color',colors(i,:),'MarkerSize',10,'MarkerFaceColor',colors(i,:));
    text(mp(1)+0.06,mp(2)+0.06,mp(3)+0.04,sprintf('M%d',i),'Color',colors(i,:),'FontSize',9,'FontWeight','bold');
end
plot3([0 u_unit(1)],[0 u_unit(2)],[0 0],'r:','LineWidth',1);
plot3(u_unit(1),u_unit(2),0,'r+','MarkerSize',12,'LineWidth',2);
quiver3(0,0,0,u_unit(1),u_unit(2),u_unit(3),0,'r','LineWidth',4,'MaxHeadSize',0.35);
view(35,22);
xlabel('X'); ylabel('Y'); zlabel('Z');
title(sprintf('3D smjer dolaska zvuka\nAzimut: %+.1f°   |   Elevacija: %+.1f°', ...
      azimuth_deg, elevation_deg),'FontSize',11);
annotation('textbox',[0.03 0.03 0.42 0.13],'String', ...
    {sprintf('Azimut:     %+.1f°', azimuth_deg), ...
     sprintf('Elevacija:   %+.1f°', elevation_deg), ...
     sprintf('Vec: [%+.2f, %+.2f, %+.2f]', u_unit(1), u_unit(2), u_unit(3))}, ...
    'FontSize',9,'BackgroundColor','w','EdgeColor',[0.6 0.6 0.6]);

%% =========================================================================
%  LOKALNE FUNKCIJE
% =========================================================================

% Detektira kratke glitch uzorke i zamjenjuje ih linearnom interpolacijom.
% Glitch = nagla promjena amplitude u trajanju <= max_len uzoraka.
% Vraća: popravljeni signal, logičku masku glitch pozicija, broj popravljenih.
function [x, mask, n_fixed] = remove_glitches(x, diff_factor, max_len)
    mask    = false(size(x));
    n_fixed = 0;
    N       = length(x);

    d      = abs(diff(x));
    thresh = diff_factor * median(d);   % adaptivni prag po kanalu

    % Pronađi sve uzorke gdje je razlika prema susjedu prevelika
    big_jumps = find(d > thresh);       % pozicije skoka (ulaz ILI izlaz glitcha)

    k = 1;
    while k <= length(big_jumps)
        entry = big_jumps(k);           % potencijalni ulaz u glitch

        % Tražimo izlaz: sljedeći veliki skok unutar max_len koraka
        exits = big_jumps(big_jumps > entry & big_jumps <= entry + max_len);

        if ~isempty(exits)
            ex = exits(1);              % izlaz iz glitcha (entry+1 do entry+max_len)

            % Granice interpolacije: jedan uzorak s lijeve i desne strane
            left  = entry;              % zadnji "dobri" uzorak (prije glitcha)
            right = ex + 1;             % prvi "dobri" uzorak (nakon glitcha)

            if left >= 1 && right <= N
                n_bad = right - left - 1;       % broj uzoraka koje trebamo popraviti
                if n_bad >= 1 && n_bad <= max_len
                    vals = linspace(x(left), x(right), n_bad + 2);
                    x(left+1 : right-1) = vals(2:end-1);
                    mask(left+1 : right-1) = true;
                    n_fixed = n_fixed + 1;
                end
            end

            % Preskoči sve big_jumps do iza ex
            k = find(big_jumps > ex, 1);
            if isempty(k), break; end
        else
            k = k + 1;
        end
    end
end

% GCC-PHAT međukorelacija
function [xc, lags] = gcc_phat(x, y, max_l)
    N = 2^nextpow2(2*length(x));
    X = fft(x - mean(x), N);
    Y = fft(y - mean(y), N);
    G = X .* conj(Y);
    G = G ./ (abs(G) + 1e-9);
    r = real(ifft(G));
    r = [r(end-max_l+1:end); r(1:max_l+1)];
    lags = (-max_l:max_l)';
    xc   = r;
end
