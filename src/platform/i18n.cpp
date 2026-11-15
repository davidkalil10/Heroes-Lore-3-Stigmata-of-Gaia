#include "i18n.h"
#include "../core/common.h"
#include "display_config.h"

#include <unordered_map>

namespace hl3 {

namespace {

std::string g_active_language = kDefaultLanguage;

const std::unordered_map<std::string_view, std::string_view>& en_table() {
    static const std::unordered_map<std::string_view, std::string_view> kEn = {
        {"game.btn.port_options", "Port Options & Cloud"},
        {"app.title_short", "HEROES LORE 3: STIGMATA OF GAIA"},
        {"common.back", "Back"},
        {"common.close", "Close"},
        {"common.cancel", "Cancel"},
        {"common.confirm", "Confirm"},
        {"common.yes", "Yes"},
        {"common.no", "No"},
        {"common.unknown", "Unknown"},
        {"options.title", "PORT OPTIONS & CLOUD"},
        {"options.subtitle", "Display, Performance, Language, Cloud & Updates  •  Port v{version}"},
        {"options.row.fps", "Frame Rate (FPS)"},
        {"options.row.fps.desc", "Switch between 30 FPS classic pacing and 60 FPS smooth mode"},
        {"options.row.aspect", "Aspect Ratio"},
        {"options.row.aspect.desc", "Expand visible field/viewport without stretching HUD or sprites"},
        {"options.row.language", "Language"},
        {"options.row.language.desc", "Instant live switch across menus and dialogues (EN / PT-BR)"},
        {"options.row.cloud", "Cloud Save (Google Drive)"},
        {"options.row.cloud.desc", "Back up and restore saves across PC, Android and Switch"},
        {"options.row.startup_updates", "Check Updates at Startup"},
        {"options.row.startup_updates.desc", "Silently check GitHub Releases for new port versions on launch"},
        {"options.row.updates", "System Updates (GitHub)"},
        {"options.row.updates.desc", "Check releases, download packages and view patch notes"},
        {"options.val.fps.30", "30 FPS (Standard)"},
        {"options.val.fps.60", "60 FPS (Smooth)"},
        {"options.val.aspect.auto", "Auto (Full Screen)"},
        {"options.val.aspect.16:9", "16:9 Widescreen"},
        {"options.val.aspect.original", "Original (3:2)"},
        {"options.val.lang.en", "English (US)"},
        {"options.val.lang.pt-BR", "Português (BR)"},
        {"options.val.cloud.connected", "Connected ›"},
        {"options.val.cloud.disconnected", "Manage ›"},
        {"options.val.toggle.on", "Enabled"},
        {"options.val.toggle.off", "Disabled"},
        {"options.val.updates.available", "Update {tag}!"},
        {"options.val.updates.checking", "Checking..."},
        {"options.val.updates.check", "v{version} ›"},
        {"options.footer.hints", "Arrows / D-Pad: Navigate   •   Left / Right / Enter: Change   •   Esc / B: Back"},
        {"options.footer.hints.compact", "Tap or use Keys/Gamepad  •  [F8] FPS  [F9] Aspect"},
        {"cloud.title", "CLOUD SAVE (GOOGLE DRIVE)"},
        {"cloud.subtitle", "Folder: Heroes Lore III - Stigmata of Gaia"},
        {"cloud.status.connected", "Connected to Google Drive"},
        {"cloud.status.disconnected", "Not connected"},
        {"cloud.status.authorizing", "Waiting for Google authorization..."},
        {"cloud.status.busy", "Syncing with Google Drive..."},
        {"cloud.card.remote.title", "Cloud Backup"},
        {"cloud.card.local.title", "Local Save (This Device)"},
        {"cloud.card.remote.none", "No cloud backup found yet."},
        {"cloud.card.remote.unknown", "Connect Google to inspect your cloud save."},
        {"cloud.card.local.none", "No local save files on this device."},
        {"cloud.field.date", "Updated"},
        {"cloud.field.summary", "Details"},
        {"cloud.field.platform", "Platform"},
        {"cloud.auth.step", "1) Open on phone or PC:"},
        {"cloud.auth.code_label", "2) Enter this code:"},
        {"cloud.auth.waiting", "Checking automatically every few seconds..."},
        {"cloud.auth.open_browser", "Open Browser"},
        {"cloud.btn.connect", "Connect Google"},
        {"cloud.btn.upload", "Send to Cloud"},
        {"cloud.btn.restore", "Restore Save"},
        {"cloud.btn.refresh", "Refresh"},
        {"cloud.btn.disconnect", "Disconnect"},
        {"cloud.btn.cancel_auth", "Cancel Login"},
        {"cloud.footer.hints", "Arrows / D-Pad: Select Action   •   R: Refresh   •   Esc / B: Back"},
        {"cloud.msg.ready_disconnected", "Connect your Google account to back up or restore your save."},
        {"cloud.msg.ready_connected", "Ready to back up or restore via Google Drive."},
        {"cloud.msg.requesting_code", "Requesting Google sign-in code..."},
        {"cloud.msg.open_code", "Visit the URL below and enter the authorization code."},
        {"cloud.msg.connected", "Google Drive connected!"},
        {"cloud.msg.auth_cancelled", "Google sign-in cancelled or expired."},
        {"cloud.msg.checking", "Checking cloud backup..."},
        {"cloud.msg.found", "Cloud backup found."},
        {"cloud.msg.not_found", "No cloud backup for Heroes Lore 3 yet."},
        {"cloud.msg.uploading", "Uploading save files to Google Drive..."},
        {"cloud.msg.uploaded", "Backup saved to Google Drive!"},
        {"cloud.msg.downloading", "Downloading & verifying cloud backup..."},
        {"cloud.msg.restored_reloading", "Backup verified and restored! Returning to Title..."},
        {"cloud.msg.disconnected", "Disconnected from Google Drive."},
        {"cloud.msg.no_local_save", "Create or save a game first before uploading."},
        {"cloud.msg.conflict_changed", "Cloud backup changed on another device. Review its date and confirm again."},
        {"cloud.summary.slots", "{slots} save slot(s)  •  {files} file(s)  •  {kb} KB"},
        {"cloud.summary.files_only", "{files} save file(s)  •  {kb} KB"},
        {"confirm.upload.title", "Overwrite Cloud Backup?"},
        {"confirm.upload.body", "A backup from {date} already exists on Google Drive.\nSending now will replace it with your local save."},
        {"confirm.upload.body_new", "Upload your current local save files to Google Drive?"},
        {"confirm.restore.title", "Restore Cloud Backup?"},
        {"confirm.restore.body", "Your current local save will be backed up to save-backups/, replaced by the cloud backup ({date}), and the game will return safely to the Title Screen."},
        {"confirm.disconnect.title", "Disconnect Google Drive?"},
        {"confirm.disconnect.body", "This signs out on this device. Your files on Google Drive remain untouched."},
        {"confirm.action.upload", "Send Backup"},
        {"confirm.action.restore", "Restore Now"},
        {"confirm.action.disconnect", "Disconnect"},
        {"osd.fps.60.title", "FRAME RATE: 60 FPS"},
        {"osd.fps.60.sub", "High Smoothness Mode  •  [F8] toggle"},
        {"osd.fps.30.title", "FRAME RATE: 30 FPS"},
        {"osd.fps.30.sub", "Calibrated Standard Mode  •  [F8] toggle"},
        {"osd.aspect.auto.badge", "AUTO"},
        {"osd.aspect.auto.title", "ASPECT RATIO: AUTO (FULL WIDE)"},
        {"osd.aspect.auto.sub", "Fills full window without stretching  •  [F9] cycle"},
        {"osd.aspect.16:9.badge", "16:9"},
        {"osd.aspect.16:9.title", "ASPECT RATIO: 16:9 WIDESCREEN"},
        {"osd.aspect.16:9.sub", "Expanded horizontal view  •  [F9] cycle"},
        {"osd.aspect.original.badge", "3:2"},
        {"osd.aspect.original.title", "ASPECT RATIO: ORIGINAL (3:2)"},
        {"osd.aspect.original.sub", "Classic 480x320 framing  •  [F9] cycle"},
        {"osd.fullscreen.on.title", "FULLSCREEN ENABLED"},
        {"osd.fullscreen.off.title", "WINDOWED MODE ENABLED"},
        {"osd.fullscreen.sub", "Aspect {aspect}  •  [F11] toggle"},
        {"osd.fullscreen.badge", "VIEW"},
        {"osd.lang.badge", "LANG"},
        {"osd.lang.title", "LANGUAGE: ENGLISH (US)"},
        {"osd.lang.sub", "Applied live without restarting"},
        {"osd.restore.badge", "CLOUD"},
        {"osd.restore.title", "SAVE RESTORED"},
        {"osd.restore.sub", "Loaded backup from Google Drive"},
        {"osd.update.badge", "UPDATE"},
        {"osd.update.title", "NEW VERSION AVAILABLE: {tag}"},
        {"osd.update.sub", "Open Port Options & Cloud -> System Updates to download"},
        {"update.title", "SYSTEM UPDATES (GITHUB)"},
        {"update.subtitle", "Installed: v{version}  •  Original Game: {original}  •  {repo}"},
        {"update.card.version_title", "Version Status"},
        {"update.card.notes_title", "Release Notes"},
        {"update.field.installed", "Installed Port: v{version} (David Kalil Braga, 2026)"},
        {"update.field.remote", "Latest Release: {tag}"},
        {"update.field.asset", "Package: {name}"},
        {"update.field.none", "Waiting for release check..."},
        {"update.notes.empty", "Check GitHub Releases to inspect release notes and available updates."},
        {"update.status.idle", "Press 'Check Updates' to query GitHub Releases."},
        {"update.status.checking", "Checking GitHub Releases for updates..."},
        {"update.status.up_to_date", "You are running the latest version (v{version})."},
        {"update.status.available", "New version available: {tag} (current: v{version})."},
        {"update.status.no_release", "No public release published on GitHub yet (404 / Draft)."},
        {"update.status.error", "Update check failed: {detail}"},
        {"update.status.downloading", "Downloading {name}: {mb_done} / {mb_total} MB ({pct}%)"},
        {"update.status.downloading_unknown", "Downloading {name}: {mb_done} MB..."},
        {"update.status.ready", "Package verified cleanly! Ready to install."},
        {"update.status.installing", "Applying update and restarting..."},
        {"update.status.manual_staged", "Package verified at {path}"},
        {"update.btn.check", "Check Updates"},
        {"update.btn.download", "Download {tag}"},
        {"update.btn.install", "Install & Restart"},
        {"update.btn.open_staged", "Open Folder"},
        {"update.btn.github", "Open GitHub"},
        {"update.footer.hints", "Enter: Primary Action   •   R: Check Again   •   Esc / B: Back"},
    };
    return kEn;
}

const std::unordered_map<std::string_view, std::string_view>& ptbr_table() {
    static const std::unordered_map<std::string_view, std::string_view> kPt = {
        {"game.btn.port_options", "Opções & Cloud Save"},
        {"app.title_short", "HEROES LORE 3: STIGMATA OF GAIA"},
        {"common.back", "Voltar"},
        {"common.close", "Fechar"},
        {"common.cancel", "Cancelar"},
        {"common.confirm", "Confirmar"},
        {"common.yes", "Sim"},
        {"common.no", "Não"},
        {"common.unknown", "Desconhecido"},
        {"options.title", "OPÇÕES DO PORT & CLOUD"},
        {"options.subtitle", "Vídeo, Desempenho, Idioma, Nuvem e Atualizações  •  Port v{version}"},
        {"options.row.fps", "Taxa de Quadros (FPS)"},
        {"options.row.fps.desc", "Alterna entre 30 FPS clássico e 60 FPS alta fluidez"},
        {"options.row.aspect", "Proporção da Tela"},
        {"options.row.aspect.desc", "Expande o campo visível sem esticar HUD ou sprites"},
        {"options.row.language", "Idioma / Language"},
        {"options.row.language.desc", "Troca instantânea em menus e diálogos (EN / PT-BR)"},
        {"options.row.cloud", "Cloud Save (Google Drive)"},
        {"options.row.cloud.desc", "Envie e restaure saves entre PC, Android e Switch"},
        {"options.row.startup_updates", "Buscar Atualizações ao Iniciar"},
        {"options.row.startup_updates.desc", "Verifica novas releases no GitHub em segundo plano ao abrir"},
        {"options.row.updates", "Atualizações do Sistema (GitHub)"},
        {"options.row.updates.desc", "Verificar novas versões, baixar pacotes e ver notas"},
        {"options.val.fps.30", "30 FPS (Padrão)"},
        {"options.val.fps.60", "60 FPS (Alta Fluidez)"},
        {"options.val.aspect.auto", "Auto (Tela Cheia)"},
        {"options.val.aspect.16:9", "16:9 Widescreen"},
        {"options.val.aspect.original", "Original (3:2)"},
        {"options.val.lang.en", "English (US)"},
        {"options.val.lang.pt-BR", "Português (BR)"},
        {"options.val.cloud.connected", "Conectado ›"},
        {"options.val.cloud.disconnected", "Gerenciar ›"},
        {"options.val.toggle.on", "Ativado"},
        {"options.val.toggle.off", "Desativado"},
        {"options.val.updates.available", "Nova {tag}!"},
        {"options.val.updates.checking", "Buscando..."},
        {"options.val.updates.check", "v{version} ›"},
        {"options.footer.hints", "Setas / D-Pad: Navegar   •   Esq / Dir / Enter: Alterar   •   Esc / B: Voltar"},
        {"options.footer.hints.compact", "Toque ou Setas/Controle  •  [F8] FPS  [F9] Proporção"},
        {"cloud.title", "CLOUD SAVE (GOOGLE DRIVE)"},
        {"cloud.subtitle", "Pasta: Heroes Lore III - Stigmata of Gaia"},
        {"cloud.status.connected", "Conectado ao Google Drive"},
        {"cloud.status.disconnected", "Desconectado"},
        {"cloud.status.authorizing", "Aguardando autorização do Google..."},
        {"cloud.status.busy", "Sincronizando com Google Drive..."},
        {"cloud.card.remote.title", "Backup na Nuvem"},
        {"cloud.card.local.title", "Save Local (Este Dispositivo)"},
        {"cloud.card.remote.none", "Nenhum backup encontrado na nuvem."},
        {"cloud.card.remote.unknown", "Conecte o Google para verificar seu save."},
        {"cloud.card.local.none", "Nenhum save local encontrado."},
        {"cloud.field.date", "Atualizado"},
        {"cloud.field.summary", "Resumo"},
        {"cloud.field.platform", "Origem"},
        {"cloud.auth.step", "1) Acesse no celular ou PC:"},
        {"cloud.auth.code_label", "2) Digite o código abaixo:"},
        {"cloud.auth.waiting", "Verificando autorização automaticamente..."},
        {"cloud.auth.open_browser", "Abrir Navegador"},
        {"cloud.btn.connect", "Conectar Google"},
        {"cloud.btn.upload", "Enviar p/ Nuvem"},
        {"cloud.btn.restore", "Restaurar Save"},
        {"cloud.btn.refresh", "Atualizar"},
        {"cloud.btn.disconnect", "Desconectar"},
        {"cloud.btn.cancel_auth", "Cancelar Login"},
        {"cloud.footer.hints", "Setas / D-Pad: Escolher Ação   •   R: Atualizar   •   Esc / B: Voltar"},
        {"cloud.msg.ready_disconnected", "Conecte sua conta Google para enviar ou restaurar seu progresso."},
        {"cloud.msg.ready_connected", "Pronto para enviar ou restaurar seu save no Google Drive."},
        {"cloud.msg.requesting_code", "Solicitando código de login do Google..."},
        {"cloud.msg.open_code", "Acesse o endereço abaixo e digite o código."},
        {"cloud.msg.connected", "Google Drive conectado!"},
        {"cloud.msg.auth_cancelled", "Login Google cancelado ou expirado."},
        {"cloud.msg.checking", "Verificando backup na nuvem..."},
        {"cloud.msg.found", "Backup na nuvem encontrado."},
        {"cloud.msg.not_found", "Ainda não há backup de Heroes Lore 3."},
        {"cloud.msg.uploading", "Enviando arquivos de save para o Google Drive..."},
        {"cloud.msg.uploaded", "Backup enviado ao Google Drive!"},
        {"cloud.msg.downloading", "Baixando e validando backup da nuvem..."},
        {"cloud.msg.restored_reloading", "Backup validado e restaurado! Voltando ao Título..."},
        {"cloud.msg.disconnected", "Desconectado do Google Drive."},
        {"cloud.msg.no_local_save", "Crie ou salve um jogo antes de enviar para a nuvem."},
        {"cloud.msg.conflict_changed", "O backup na nuvem mudou em outro aparelho. Confira a data e confirme novamente."},
        {"cloud.summary.slots", "{slots} slot(s) salvo(s)  •  {files} arquivo(s)  •  {kb} KB"},
        {"cloud.summary.files_only", "{files} arquivo(s) de save  •  {kb} KB"},
        {"confirm.upload.title", "Sobrescrever Backup na Nuvem?"},
        {"confirm.upload.body", "Já existe um backup de {date} no Google Drive.\nAo enviar agora, ele será substituído pelo save deste dispositivo."},
        {"confirm.upload.body_new", "Enviar os arquivos de save locais atuais para o Google Drive?"},
        {"confirm.restore.title", "Restaurar Backup da Nuvem?"},
        {"confirm.restore.body", "Seu save local atual será copiado para save-backups/, substituído pelo backup de {date}, e o jogo voltará com segurança para a tela de Título."},
        {"confirm.disconnect.title", "Desconectar Google Drive?"},
        {"confirm.disconnect.body", "Isso remove a conta apenas deste dispositivo. Seus arquivos no Google Drive continuarão salvos."},
        {"confirm.action.upload", "Enviar Agora"},
        {"confirm.action.restore", "Restaurar"},
        {"confirm.action.disconnect", "Desconectar"},
        {"osd.fps.60.title", "TAXA DE QUADROS: 60 FPS"},
        {"osd.fps.60.sub", "Modo Alta Fluidez  •  [F8] alternar"},
        {"osd.fps.30.title", "TAXA DE QUADROS: 30 FPS"},
        {"osd.fps.30.sub", "Modo Padrão Calibrado  •  [F8] alternar"},
        {"osd.aspect.auto.badge", "AUTO"},
        {"osd.aspect.auto.title", "PROPORÇÃO: AUTO (FULL WIDE)"},
        {"osd.aspect.auto.sub", "Preenche toda a tela sem esticar  •  [F9] alternar"},
        {"osd.aspect.16:9.badge", "16:9"},
        {"osd.aspect.16:9.title", "PROPORÇÃO: 16:9 WIDESCREEN"},
        {"osd.aspect.16:9.sub", "Visão horizontal expandida  •  [F9] alternar"},
        {"osd.aspect.original.badge", "3:2"},
        {"osd.aspect.original.title", "PROPORÇÃO: ORIGINAL (3:2)"},
        {"osd.aspect.original.sub", "Enquadramento clássico 480x320  •  [F9] alternar"},
        {"osd.fullscreen.on.title", "TELA CHEIA ATIVADA"},
        {"osd.fullscreen.off.title", "MODO JANELA ATIVADO"},
        {"osd.fullscreen.sub", "Proporção {aspect}  •  [F11] alternar"},
        {"osd.fullscreen.badge", "TELA"},
        {"osd.lang.badge", "IDIOMA"},
        {"osd.lang.title", "IDIOMA: PORTUGUÊS (BR)"},
        {"osd.lang.sub", "Alterado em tempo real sem reiniciar"},
        {"osd.restore.badge", "NUVEM"},
        {"osd.restore.title", "SAVE RESTAURADO"},
        {"osd.restore.sub", "Backup carregado do Google Drive"},
        {"osd.update.badge", "UPDATE"},
        {"osd.update.title", "NOVA VERSÃO DISPONÍVEL: {tag}"},
        {"osd.update.sub", "Abra Opções do Port -> Atualizações do Sistema para baixar"},
        {"update.title", "ATUALIZAÇÕES DO SISTEMA (GITHUB)"},
        {"update.subtitle", "Instalado: v{version}  •  Jogo Original: {original}  •  {repo}"},
        {"update.card.version_title", "Versão do Port"},
        {"update.card.notes_title", "Notas da Versão"},
        {"update.field.installed", "Port Instalado: v{version} (David Kalil Braga, 2026)"},
        {"update.field.remote", "Última Release: {tag}"},
        {"update.field.asset", "Pacote: {name}"},
        {"update.field.none", "Aguardando verificação de atualizações..."},
        {"update.notes.empty", "Busque no GitHub Releases para visualizar as notas e baixar atualizações."},
        {"update.status.idle", "Clique em 'Verificar Agora' para consultar o GitHub Releases."},
        {"update.status.checking", "Consultando o GitHub Releases..."},
        {"update.status.up_to_date", "Você já está na versão mais recente (v{version})."},
        {"update.status.available", "Nova versão disponível: {tag} (instalada: v{version})."},
        {"update.status.no_release", "Nenhuma release pública publicada no GitHub ainda (404)."},
        {"update.status.error", "Erro ao verificar atualizações: {detail}"},
        {"update.status.downloading", "Baixando {name}: {mb_done} / {mb_total} MB ({pct}%)"},
        {"update.status.downloading_unknown", "Baixando {name}: {mb_done} MB..."},
        {"update.status.ready", "Pacote baixado e verificado! Pronto para instalar."},
        {"update.status.installing", "Aplicando atualização e reiniciando..."},
        {"update.status.manual_staged", "Pacote validado salvo em {path}"},
        {"update.btn.check", "Verificar Agora"},
        {"update.btn.download", "Baixar {tag}"},
        {"update.btn.install", "Instalar e Reiniciar"},
        {"update.btn.open_staged", "Abrir Pasta"},
        {"update.btn.github", "Ver no GitHub"},
        {"update.footer.hints", "Enter: Ação Principal   •   R: Verificar   •   Esc / B: Voltar"},
    };
    return kPt;
}

}  // namespace

std::string get_target_asset_name() {
#if defined(_WIN32)
    return kAssetWindowsX64;
#elif defined(__ANDROID__)
    return kAssetAndroidApk;
#elif defined(__SWITCH2__)
    return "heroes_lore_3_switch2.nro";
#elif defined(__SWITCH__)
    return kAssetSwitchNro;
#else
    return kAssetLinuxX64;
#endif
}

std::string get_port_tag(std::string_view version) {
    std::string clean(version.empty() ? std::string_view(kPortVersion) : version);
    clean = trim_ascii(clean);
    if (!clean.empty() && (clean[0] == 'v' || clean[0] == 'V')) {
        clean.erase(clean.begin());
    }
    return "v" + clean;
}

std::string about_credit_prefix(std::string_view lang, std::string_view version) {
    std::string ver = version.empty() ? std::string(kPortVersion) : std::string(version);
    if (normalize_language(std::string(lang)) == "pt-BR") {
        return std::string("Port por ") + kAboutAuthor + " (" + kAboutYear + ") - v" + ver + "||";
    }
    return std::string("Port by ") + kAboutAuthor + " (" + kAboutYear + ") - v" + ver + "||";
}

std::string set_active_i18n_language(std::string_view lang) {
    g_active_language = normalize_language(std::string(lang));
    return g_active_language;
}

const std::string& get_active_i18n_language() {
    return g_active_language;
}

std::string tr(std::string_view key, std::string_view lang) {
    std::string norm = lang.empty() ? g_active_language : normalize_language(std::string(lang));
    if (norm == "pt-BR") {
        const auto& pt = ptbr_table();
        if (auto it = pt.find(key); it != pt.end()) return std::string(it->second);
    }
    const auto& en = en_table();
    if (auto it = en.find(key); it != en.end()) return std::string(it->second);
    return std::string(key);
}

std::string tr_fmt(std::string_view key,
                   std::initializer_list<std::pair<std::string_view, std::string_view>> vars,
                   std::string_view lang) {
    std::string out = tr(key, lang);
    for (const auto& kv : vars) {
        std::string needle = "{" + std::string(kv.first) + "}";
        size_t pos = 0;
        while ((pos = out.find(needle, pos)) != std::string::npos) {
            out.replace(pos, needle.size(), kv.second);
            pos += kv.second.size();
        }
    }
    return out;
}

}  // namespace hl3

