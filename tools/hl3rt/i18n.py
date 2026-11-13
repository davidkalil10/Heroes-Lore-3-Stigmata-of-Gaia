"""Bilingual localization registry (EN / PT-BR) for Heroes Lore 3 Recomp port UI & runtime.

Per project engineering rules, English ("en") is the default boot language and
Portuguese ("pt-BR") is switchable live at runtime without restarting the app or
affecting saved data. This module also exposes the canonical active-language
accessor so future S09/S10 game-text localization can plug directly into it.
"""

from __future__ import annotations

from .display import DEFAULT_LANGUAGE, VALID_LANGUAGES, normalize_language

_ACTIVE_LANGUAGE: str = DEFAULT_LANGUAGE

STRINGS: dict[str, dict[str, str]] = {
    "en": {
        # Injected native menu button labels (rendered onto native silver-blue ribbon)
        "game.btn.port_options": "Port Options & Cloud",
        # Shared
        "app.title_short": "HEROES LORE 3: STIGMATA OF GAIA",
        "common.back": "Back",
        "common.close": "Close",
        "common.cancel": "Cancel",
        "common.confirm": "Confirm",
        "common.yes": "Yes",
        "common.no": "No",
        "common.unknown": "Unknown",
        # Port Options Menu
        "options.title": "PORT OPTIONS & CLOUD",
        "options.subtitle": "Display, Performance, Language & Cloud Backup",
        "options.row.fps": "Frame Rate (FPS)",
        "options.row.fps.desc": "Switch between 30 FPS classic pacing and 60 FPS smooth mode",
        "options.row.aspect": "Aspect Ratio",
        "options.row.aspect.desc": "Expand visible field/viewport without stretching HUD or sprites",
        "options.row.language": "Language",
        "options.row.language.desc": "Live UI language (game text localization ready for S09/S10)",
        "options.row.cloud": "Cloud Save (Google Drive)",
        "options.row.cloud.desc": "Back up and restore saves across PC, Android and Switch",
        "options.val.fps.30": "30 FPS (Standard)",
        "options.val.fps.60": "60 FPS (Smooth)",
        "options.val.aspect.auto": "Auto (Full Screen)",
        "options.val.aspect.16:9": "16:9 Widescreen",
        "options.val.aspect.original": "Original (3:2)",
        "options.val.lang.en": "English (US)",
        "options.val.lang.pt-BR": "Português (BR)",
        "options.val.cloud.connected": "Connected ›",
        "options.val.cloud.disconnected": "Manage ›",
        "options.footer.hints": "Arrows / D-Pad: Navigate   •   Left / Right / Enter: Change   •   Esc / B: Back",
        "options.footer.hints.compact": "Tap or use Keys/Gamepad  •  [F8] FPS  [F9] Aspect",
        # Cloud Save Screen
        "cloud.title": "CLOUD SAVE (GOOGLE DRIVE)",
        "cloud.subtitle": "Folder: Heroes Lore III - Stigmata of Gaia",
        "cloud.status.connected": "Connected to Google Drive",
        "cloud.status.disconnected": "Not connected",
        "cloud.status.authorizing": "Waiting for Google authorization...",
        "cloud.status.busy": "Syncing with Google Drive...",
        "cloud.card.remote.title": "Cloud Backup",
        "cloud.card.local.title": "Local Save (This Device)",
        "cloud.card.remote.none": "No cloud backup found yet.",
        "cloud.card.remote.unknown": "Connect Google to inspect your cloud save.",
        "cloud.card.local.none": "No local save files on this device.",
        "cloud.field.date": "Updated",
        "cloud.field.summary": "Details",
        "cloud.field.platform": "Platform",
        "cloud.auth.step": "1) Open on phone or PC:",
        "cloud.auth.code_label": "2) Enter this code:",
        "cloud.auth.waiting": "Checking automatically every few seconds...",
        "cloud.auth.open_browser": "Open Browser",
        "cloud.btn.connect": "Connect Google",
        "cloud.btn.upload": "Send to Cloud",
        "cloud.btn.restore": "Restore Save",
        "cloud.btn.refresh": "Refresh",
        "cloud.btn.disconnect": "Disconnect",
        "cloud.btn.cancel_auth": "Cancel Login",
        "cloud.footer.hints": "Arrows / D-Pad: Select Action   •   R: Refresh   •   Esc / B: Back",
        "cloud.msg.ready_disconnected": "Connect your Google account to back up or restore your save.",
        "cloud.msg.ready_connected": "Ready to back up or restore via Google Drive.",
        "cloud.msg.requesting_code": "Requesting Google sign-in code...",
        "cloud.msg.open_code": "Visit the URL below and enter the authorization code.",
        "cloud.msg.connected": "Google Drive connected!",
        "cloud.msg.auth_cancelled": "Google sign-in cancelled or expired.",
        "cloud.msg.checking": "Checking cloud backup...",
        "cloud.msg.found": "Cloud backup found.",
        "cloud.msg.not_found": "No cloud backup for Heroes Lore 3 yet.",
        "cloud.msg.uploading": "Uploading save files to Google Drive...",
        "cloud.msg.uploaded": "Backup saved to Google Drive!",
        "cloud.msg.downloading": "Downloading & verifying cloud backup...",
        "cloud.msg.restored_reloading": "Backup verified and restored! Returning to Title...",
        "cloud.msg.disconnected": "Disconnected from Google Drive.",
        "cloud.msg.no_local_save": "Create or save a game first before uploading.",
        "cloud.msg.conflict_changed": "Cloud backup changed on another device. Review its date and confirm again.",
        "cloud.summary.slots": "{slots} save slot(s)  •  {files} file(s)  •  {kb:.1f} KB",
        "cloud.summary.files_only": "{files} save file(s)  •  {kb:.1f} KB",
        # Confirm dialogs
        "confirm.upload.title": "Overwrite Cloud Backup?",
        "confirm.upload.body": "A backup from {date} already exists on Google Drive.\nSending now will replace it with your local save.",
        "confirm.upload.body_new": "Upload your current local save files to Google Drive?",
        "confirm.restore.title": "Restore Cloud Backup?",
        "confirm.restore.body": "Your current local save will be backed up to save-backups/, replaced by the cloud backup ({date}), and the game will return safely to the Title Screen.",
        "confirm.disconnect.title": "Disconnect Google Drive?",
        "confirm.disconnect.body": "This signs out on this device. Your files on Google Drive remain untouched.",
        "confirm.action.upload": "Send Backup",
        "confirm.action.restore": "Restore Now",
        "confirm.action.disconnect": "Disconnect",
        # OSD notifications
        "osd.fps.60.title": "FRAME RATE: 60 FPS",
        "osd.fps.60.sub": "High Smoothness Mode  •  [F8] toggle",
        "osd.fps.30.title": "FRAME RATE: 30 FPS",
        "osd.fps.30.sub": "Calibrated Standard Mode  •  [F8] toggle",
        "osd.aspect.auto.badge": "AUTO",
        "osd.aspect.auto.title": "ASPECT RATIO: AUTO (FULL WIDE)",
        "osd.aspect.auto.sub": "Fills full window without stretching  •  [F9] cycle",
        "osd.aspect.16:9.badge": "16:9",
        "osd.aspect.16:9.title": "ASPECT RATIO: 16:9 WIDESCREEN",
        "osd.aspect.16:9.sub": "Expanded horizontal view  •  [F9] cycle",
        "osd.aspect.original.badge": "3:2",
        "osd.aspect.original.title": "ASPECT RATIO: ORIGINAL (3:2)",
        "osd.aspect.original.sub": "Classic 480x320 framing  •  [F9] cycle",
        "osd.fullscreen.on.title": "FULLSCREEN ENABLED",
        "osd.fullscreen.off.title": "WINDOWED MODE ENABLED",
        "osd.fullscreen.sub": "Aspect {aspect}  •  [F11] toggle",
        "osd.fullscreen.badge": "VIEW",
        "osd.lang.badge": "LANG",
        "osd.lang.title": "LANGUAGE: ENGLISH (US)",
        "osd.lang.sub": "Applied live (S09/S10 text hook ready)",
        "osd.restore.badge": "CLOUD",
        "osd.restore.title": "SAVE RESTORED",
        "osd.restore.sub": "Loaded backup from Google Drive",
    },
    "pt-BR": {
        # Botão nativo injetado nos menus do jogo
        "game.btn.port_options": "Opções & Cloud Save",
        # Comuns
        "app.title_short": "HEROES LORE 3: STIGMATA OF GAIA",
        "common.back": "Voltar",
        "common.close": "Fechar",
        "common.cancel": "Cancelar",
        "common.confirm": "Confirmar",
        "common.yes": "Sim",
        "common.no": "Não",
        "common.unknown": "Desconhecido",
        # Menu de Opções do Port
        "options.title": "OPÇÕES DO PORT & CLOUD",
        "options.subtitle": "Vídeo, Desempenho, Idioma e Backup na Nuvem",
        "options.row.fps": "Taxa de Quadros (FPS)",
        "options.row.fps.desc": "Alterna entre 30 FPS clássico e 60 FPS alta fluidez",
        "options.row.aspect": "Proporção da Tela",
        "options.row.aspect.desc": "Expande o campo visível sem esticar HUD ou sprites",
        "options.row.language": "Idioma / Language",
        "options.row.language.desc": "Troca instantânea (base pronta para tradução S09/S10)",
        "options.row.cloud": "Cloud Save (Google Drive)",
        "options.row.cloud.desc": "Envie e restaure saves entre PC, Android e Switch",
        "options.val.fps.30": "30 FPS (Padrão)",
        "options.val.fps.60": "60 FPS (Alta Fluidez)",
        "options.val.aspect.auto": "Auto (Tela Cheia)",
        "options.val.aspect.16:9": "16:9 Widescreen",
        "options.val.aspect.original": "Original (3:2)",
        "options.val.lang.en": "English (US)",
        "options.val.lang.pt-BR": "Português (BR)",
        "options.val.cloud.connected": "Conectado ›",
        "options.val.cloud.disconnected": "Gerenciar ›",
        "options.footer.hints": "Setas / D-Pad: Navegar   •   Esq / Dir / Enter: Alterar   •   Esc / B: Voltar",
        "options.footer.hints.compact": "Toque ou Setas/Controle  •  [F8] FPS  [F9] Proporção",
        # Tela de Cloud Save
        "cloud.title": "CLOUD SAVE (GOOGLE DRIVE)",
        "cloud.subtitle": "Pasta: Heroes Lore III - Stigmata of Gaia",
        "cloud.status.connected": "Conectado ao Google Drive",
        "cloud.status.disconnected": "Desconectado",
        "cloud.status.authorizing": "Aguardando autorização do Google...",
        "cloud.status.busy": "Sincronizando com Google Drive...",
        "cloud.card.remote.title": "Backup na Nuvem",
        "cloud.card.local.title": "Save Local (Este Dispositivo)",
        "cloud.card.remote.none": "Nenhum backup encontrado na nuvem.",
        "cloud.card.remote.unknown": "Conecte o Google para verificar seu save.",
        "cloud.card.local.none": "Nenhum save local encontrado.",
        "cloud.field.date": "Atualizado",
        "cloud.field.summary": "Resumo",
        "cloud.field.platform": "Origem",
        "cloud.auth.step": "1) Acesse no celular ou PC:",
        "cloud.auth.code_label": "2) Digite o código abaixo:",
        "cloud.auth.waiting": "Verificando autorização automaticamente...",
        "cloud.auth.open_browser": "Abrir Navegador",
        "cloud.btn.connect": "Conectar Google",
        "cloud.btn.upload": "Enviar p/ Nuvem",
        "cloud.btn.restore": "Restaurar Save",
        "cloud.btn.refresh": "Atualizar",
        "cloud.btn.disconnect": "Desconectar",
        "cloud.btn.cancel_auth": "Cancelar Login",
        "cloud.footer.hints": "Setas / D-Pad: Escolher Ação   •   R: Atualizar   •   Esc / B: Voltar",
        "cloud.msg.ready_disconnected": "Conecte sua conta Google para enviar ou restaurar seu progresso.",
        "cloud.msg.ready_connected": "Pronto para enviar ou restaurar seu save no Google Drive.",
        "cloud.msg.requesting_code": "Solicitando código de login do Google...",
        "cloud.msg.open_code": "Acesse o endereço abaixo e digite o código.",
        "cloud.msg.connected": "Google Drive conectado!",
        "cloud.msg.auth_cancelled": "Login Google cancelado ou expirado.",
        "cloud.msg.checking": "Verificando backup na nuvem...",
        "cloud.msg.found": "Backup na nuvem encontrado.",
        "cloud.msg.not_found": "Ainda não há backup de Heroes Lore 3.",
        "cloud.msg.uploading": "Enviando arquivos de save para o Google Drive...",
        "cloud.msg.uploaded": "Backup enviado ao Google Drive!",
        "cloud.msg.downloading": "Baixando e validando backup da nuvem...",
        "cloud.msg.restored_reloading": "Backup validado e restaurado! Voltando ao Título...",
        "cloud.msg.disconnected": "Desconectado do Google Drive.",
        "cloud.msg.no_local_save": "Crie ou salve um jogo antes de enviar para a nuvem.",
        "cloud.msg.conflict_changed": "O backup na nuvem mudou em outro aparelho. Confira a data e confirme novamente.",
        "cloud.summary.slots": "{slots} slot(s) salvo(s)  •  {files} arquivo(s)  •  {kb:.1f} KB",
        "cloud.summary.files_only": "{files} arquivo(s) de save  •  {kb:.1f} KB",
        # Diálogos de Confirmação
        "confirm.upload.title": "Sobrescrever Backup na Nuvem?",
        "confirm.upload.body": "Já existe um backup de {date} no Google Drive.\nAo enviar agora, ele será substituído pelo save deste dispositivo.",
        "confirm.upload.body_new": "Enviar os arquivos de save locais atuais para o Google Drive?",
        "confirm.restore.title": "Restaurar Backup da Nuvem?",
        "confirm.restore.body": "Seu save local atual será copiado para save-backups/, substituído pelo backup de {date}, e o jogo voltará com segurança para a tela de Título.",
        "confirm.disconnect.title": "Desconectar Google Drive?",
        "confirm.disconnect.body": "Isso remove a conta apenas deste dispositivo. Seus arquivos no Google Drive continuarão salvos.",
        "confirm.action.upload": "Enviar Agora",
        "confirm.action.restore": "Restaurar",
        "confirm.action.disconnect": "Desconectar",
        # Mensagens OSD
        "osd.fps.60.title": "TAXA DE QUADROS: 60 FPS",
        "osd.fps.60.sub": "Modo Alta Fluidez  •  [F8] alternar",
        "osd.fps.30.title": "TAXA DE QUADROS: 30 FPS",
        "osd.fps.30.sub": "Modo Padrão Calibrado  •  [F8] alternar",
        "osd.aspect.auto.badge": "AUTO",
        "osd.aspect.auto.title": "PROPORÇÃO: AUTO (FULL WIDE)",
        "osd.aspect.auto.sub": "Preenche toda a tela sem esticar  •  [F9] alternar",
        "osd.aspect.16:9.badge": "16:9",
        "osd.aspect.16:9.title": "PROPORÇÃO: 16:9 WIDESCREEN",
        "osd.aspect.16:9.sub": "Visão horizontal expandida  •  [F9] alternar",
        "osd.aspect.original.badge": "3:2",
        "osd.aspect.original.title": "PROPORÇÃO: ORIGINAL (3:2)",
        "osd.aspect.original.sub": "Enquadramento clássico 480x320  •  [F9] alternar",
        "osd.fullscreen.on.title": "TELA CHEIA ATIVADA",
        "osd.fullscreen.off.title": "MODO JANELA ATIVADO",
        "osd.fullscreen.sub": "Proporção {aspect}  •  [F11] alternar",
        "osd.fullscreen.badge": "TELA",
        "osd.lang.badge": "IDIOMA",
        "osd.lang.title": "IDIOMA: PORTUGUÊS (BR)",
        "osd.lang.sub": "Aplicado em tempo real (pronto para tradução S09/S10)",
        "osd.restore.badge": "NUVEM",
        "osd.restore.title": "SAVE RESTAURADO",
        "osd.restore.sub": "Backup carregado do Google Drive",
    },
}


def set_active_language(lang: str | None) -> str:
    global _ACTIVE_LANGUAGE
    _ACTIVE_LANGUAGE = normalize_language(lang)
    return _ACTIVE_LANGUAGE


def get_active_language() -> str:
    return _ACTIVE_LANGUAGE


def t(key: str, lang: str | None = None, **kwargs) -> str:
    active = normalize_language(lang) if lang is not None else _ACTIVE_LANGUAGE
    table = STRINGS.get(active, STRINGS[DEFAULT_LANGUAGE])
    text = table.get(key)
    if text is None:
        text = STRINGS[DEFAULT_LANGUAGE].get(key, key)
    if kwargs:
        try:
            return text.format(**kwargs)
        except Exception:
            return text
    return text


__all__ = [
    "DEFAULT_LANGUAGE",
    "VALID_LANGUAGES",
    "STRINGS",
    "normalize_language",
    "set_active_language",
    "get_active_language",
    "t",
]

