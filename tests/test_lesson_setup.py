"""Installer orchestration without downloads or changes to installed tools/services."""
import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
from unittest.mock import Mock

import pytest


LESSON = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("lesson_setup", LESSON / "setup_local.py")
setup = importlib.util.module_from_spec(spec)
spec.loader.exec_module(setup)


def copy_launchers(destination):
    for name in ("install_env_mac.command", "install_env_linux.sh"):
        shutil.copy(LESSON / name, destination)
    shutil.copytree(LESSON / "scripts", destination / "scripts")


@pytest.fixture
def kit(tmp_path):
    root = tmp_path / "kit con espacios"
    root.mkdir(parents=True)
    (root / "requirements.txt").touch()
    (root / "practica").mkdir(parents=True)
    (root / "practica/clasificar_textos.py").touch()
    copy_launchers(root)
    shutil.copy(LESSON / "setup_local.py", root)
    return root


def executable(path, source):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("#!/bin/bash\nset -eu\n" + source)
    path.chmod(0o755)


def bootstrap_fakes(tmp_path, kit, *, existing_ollama, installer="install_env_linux.sh"):
    """Exercise the real shell script; replace only network/native binaries."""
    fake_bin = tmp_path / "bin"
    fake_bin.mkdir()
    trace = tmp_path / "trace"
    env = dict(os.environ, PATH=str(fake_bin) + os.pathsep + os.environ["PATH"],
               TEST_BIN=str(fake_bin), TEST_TRACE=str(trace), TEST_KIT=str(kit), TEST_INSTALLER=installer)
    # Linux branch on every host; hide any host Ollama from command -v and known paths.
    # Use a tiny wrapper to override discovery in just the shell under test.
    wrapper = tmp_path / "run.sh"
    wrapper.write_text('''function command() {
    if [[ "${1:-}" == "-v" && "${2:-}" == "ollama" ]]; then
        [[ -f "$TEST_BIN/ollama" ]] && echo "$TEST_BIN/ollama"
    else builtin command "$@"; fi
}
source "$TEST_KIT/$TEST_INSTALLER"
''')
    # The candidates /usr/bin/ollama and /usr/local/bin/ollama are absent on this
    # test host; use existing_ollama=True to test reuse independently on other hosts.
    executable(fake_bin / "uname", 'echo Linux\n')
    executable(fake_bin / "curl", '''echo download >> "$TEST_TRACE"
while [[ "$1" != "-o" ]]; do shift; done
cp "$TEST_BIN/installer-template" "$2"
''')
    # Both official installers are simulated by the same small downloaded script.
    (fake_bin / "installer-template").write_text('''#!/bin/sh
case "$0" in
 *uv-install.sh)
   mkdir -p "$UV_UNMANAGED_INSTALL"
   cp "$TEST_BIN/uv-template" "$UV_UNMANAGED_INSTALL/uv"
   chmod +x "$UV_UNMANAGED_INSTALL/uv" ;;
 *) printf '#!/bin/sh\\nexit 0\\n' > "$TEST_BIN/ollama"
    chmod +x "$TEST_BIN/ollama"
    echo install-ollama >> "$TEST_TRACE" ;;
esac
''')
    executable(fake_bin / "uv-template", '''echo "uv:$*" >> "$TEST_TRACE"
if [[ "$1" == venv ]]; then
    target="${@: -1}"
    mkdir -p "$target/bin"
    cp "$TEST_BIN/python-template" "$target/bin/python"
    chmod +x "$target/bin/python"
fi
''')
    executable(fake_bin / "python-template", '''[[ "$1" == "$TEST_KIT/setup_local.py" ]]
echo second-stage >> "$TEST_TRACE"
''')
    if existing_ollama:
        executable(fake_bin / "ollama", "exit 0\n")
    return wrapper, env, trace


@pytest.mark.parametrize("installer", ["install_env_mac.command", "install_env_linux.sh"])
def test_shell_bootstrap_without_python_and_repeated_run(tmp_path, kit, installer):
    wrapper, env, trace = bootstrap_fakes(tmp_path, kit, existing_ollama=True, installer=installer)
    for _ in range(2):
        result = subprocess.run(["bash", str(wrapper)], env=env, cwd=tmp_path,
                                capture_output=True, text=True)
        assert result.returncode == 0, result.stdout + result.stderr
    lines = trace.read_text().splitlines()
    assert lines.count("download") == 1  # uv only, no Ollama reinstall
    assert lines.count("uv:python install 3.11") == 1
    assert lines.count("second-stage") == 2


def test_repository_layout_runs_without_packaging(tmp_path, kit):
    # The root has spaces and no parent project; invoking from elsewhere works.
    assert setup.repository_directory(kit) == kit
    wrapper, env, trace = bootstrap_fakes(tmp_path, kit, existing_ollama=True)
    result = subprocess.run(["bash", str(wrapper)], env=env, cwd=tmp_path,
                            capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
    assert (kit / ".venv/bin/python").is_file()
    assert not (kit / "servidor").exists()
    assert "second-stage" in trace.read_text()


def test_never_uses_a_parent_server(tmp_path, kit):
    nested = kit / "classroom/empty"
    nested.mkdir(parents=True)
    copy_launchers(nested)
    with pytest.raises(RuntimeError, match="estructura completa"):
        setup.repository_directory(nested)
    result = subprocess.run(["bash", str(nested / "install_env_linux.sh")],
                            cwd=tmp_path, capture_output=True, text=True)
    assert result.returncode != 0
    assert "estructura completa" in result.stderr
    assert not (kit / ".tools").exists()


def test_missing_server_fails_without_installing(tmp_path):
    with pytest.raises(RuntimeError, match="estructura completa"):
        setup.repository_directory(tmp_path / "missing")


def test_shell_download_failure_stops_before_second_stage(tmp_path, kit):
    wrapper, env, trace = bootstrap_fakes(tmp_path, kit, existing_ollama=True)
    executable(tmp_path / "bin/curl", "exit 22\n")
    result = subprocess.run(["bash", str(wrapper)], env=env, capture_output=True, text=True)
    assert result.returncode != 0
    assert "preparacion no termino" in result.stderr
    assert not trace.exists()


def test_shell_preserves_incomplete_venv(tmp_path, kit):
    wrapper, env, trace = bootstrap_fakes(tmp_path, kit, existing_ollama=True)
    sentinel = kit / ".venv/keep.txt"
    sentinel.parent.mkdir()
    sentinel.write_text("preserve")
    result = subprocess.run(["bash", str(wrapper)], env=env, capture_output=True, text=True)
    assert result.returncode != 0 and "incompleto" in result.stderr
    assert sentinel.read_text() == "preserve"
    assert "second-stage" not in trace.read_text()


def test_ready_server_is_reused_without_spawning(monkeypatch, kit):
    monkeypatch.setattr(setup, "ready", lambda: True)
    spawn = Mock()
    monkeypatch.setattr(setup.subprocess, "Popen", spawn)
    setup.ensure_ollama("ollama", kit)
    spawn.assert_not_called()


def test_own_server_starts_locally_and_stays_running(monkeypatch, kit):
    statuses = iter([False, True])
    monkeypatch.setattr(setup, "ready", lambda: next(statuses))
    monkeypatch.setenv("OLLAMA_HOST", "https://example.invalid")
    process = Mock()
    spawn = Mock(return_value=process)
    monkeypatch.setattr(setup.subprocess, "Popen", spawn)
    setup.ensure_ollama("/path with spaces/ollama", kit)
    assert spawn.call_args.args[0] == ["/path with spaces/ollama", "serve"]
    assert spawn.call_args.kwargs["env"]["OLLAMA_HOST"] == setup.OLLAMA_URL
    process.terminate.assert_not_called()


def test_start_timeout_cleans_up_only_owned_process(monkeypatch, kit):
    monkeypatch.setattr(setup, "ready", lambda: False)
    process = Mock()
    process.poll.return_value = None
    monkeypatch.setattr(setup.subprocess, "Popen", Mock(return_value=process))
    with pytest.raises(RuntimeError, match="ollama-setup.log"):
        setup.ensure_ollama("ollama", kit, timeout=0)
    process.terminate.assert_called_once()


@pytest.mark.parametrize("pull_fails,models", [(True, []), (False, [])])
def test_main_never_reports_success_when_model_missing(monkeypatch, kit, capsys, pull_fails, models):
    monkeypatch.setattr(setup, "__file__", str(kit / "setup_local.py"))
    monkeypatch.setattr(setup, "find_ollama", lambda: "ollama")
    monkeypatch.setattr(setup, "ensure_ollama", lambda *args: None)
    monkeypatch.setattr(setup, "ollama_tags", lambda: {"models": models})
    def run(args, **kwargs):
        if pull_fails and args[1] == "pull":
            raise subprocess.CalledProcessError(1, args)
    monkeypatch.setattr(setup.subprocess, "run", run)
    with pytest.raises((RuntimeError, subprocess.CalledProcessError)):
        setup.main([])
    assert "Preparado." not in capsys.readouterr().out


def test_main_pulls_on_local_host_and_checks_model(monkeypatch, kit, capsys):
    monkeypatch.setattr(setup, "__file__", str(kit / "setup_local.py"))
    monkeypatch.setattr(setup, "find_ollama", lambda: "ollama")
    monkeypatch.setattr(setup, "ensure_ollama", lambda *args: None)
    monkeypatch.setattr(setup, "ollama_tags", lambda: {"models": [{"name": setup.MODEL}]})
    check = Mock()
    monkeypatch.setattr(setup, "check_model", check)
    run = Mock()
    monkeypatch.setattr(setup.subprocess, "run", run)
    setup.main([])
    pull = next(call for call in run.call_args_list if call.args[0][1] == "pull")
    assert pull.kwargs["env"]["OLLAMA_HOST"] == setup.OLLAMA_URL
    assert "Preparado." in capsys.readouterr().out
    check.assert_called_once_with("ollama")


@pytest.mark.parametrize("size_vram", [0, 500, 1000])
def test_preload_allows_cpu_partial_and_full_gpu(monkeypatch, size_vram):
    request = Mock(side_effect=[{"done": True}, {"models": [
        {"name": setup.MODEL, "size": 1000, "size_vram": size_vram}]}])
    monkeypatch.setattr(setup, "ollama_request", request)
    run = Mock()
    monkeypatch.setattr(setup.subprocess, "run", run)
    setup.check_model("ollama")
    payload = request.call_args_list[0].args[1]
    assert payload == {"model": setup.MODEL, "stream": False, "keep_alive": "5m"}
    assert run.call_args.args[0] == ["ollama", "ps"]
    assert run.call_args.kwargs["env"]["OLLAMA_HOST"] == setup.OLLAMA_URL


@pytest.mark.parametrize("responses", [
    [{"done": False}],
    [{"done": True}, {"models": []}],
    [{"done": True}, {"models": [{"name": "otro-modelo"}]}],
])
def test_preload_must_confirm_requested_model(monkeypatch, responses):
    monkeypatch.setattr(setup, "ollama_request", Mock(side_effect=responses))
    run = Mock()
    monkeypatch.setattr(setup.subprocess, "run", run)
    with pytest.raises(RuntimeError):
        setup.check_model("ollama")
    run.assert_not_called()


def test_diagnostic_skips_install_and_pull(monkeypatch, kit):
    monkeypatch.setattr(setup, "__file__", str(kit / "setup_local.py"))
    monkeypatch.setattr(setup, "find_ollama", lambda: "ollama")
    monkeypatch.setattr(setup, "ensure_ollama", Mock())
    check = Mock()
    monkeypatch.setattr(setup, "check_model", check)
    run = Mock()
    monkeypatch.setattr(setup.subprocess, "run", run)
    setup.main(["--check-model"])
    check.assert_called_once_with("ollama")
    run.assert_not_called()
