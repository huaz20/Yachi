import os
import sys
import argparse
import subprocess

#输出设定为UTF-8
if sys.stdout.encoding != 'utf-8':
    sys.stdout.reconfigure(encoding='utf-8')

#在导入任何音频库之前，将 FFmpeg 强行塞进系统 PATH
runtime_dir = os.path.dirname(sys.executable)
ffmpeg_bin = os.path.join(runtime_dir, "ffmpeg-2026-05-06-git-f2e5eff3ff-essentials_build", "bin")
if os.path.exists(ffmpeg_bin):
    os.environ["PATH"] = ffmpeg_bin + os.pathsep + os.environ["PATH"]

#屏蔽警告
os.environ["HF_HUB_DISABLE_SYMLINKS_WARNING"] = "1"
os.environ["HF_ENDPOINT"] = "https://hf-mirror.com" 

def install_dependencies():
    """自动升级核心组件，解决 UVR5 MD5 校验失败问题"""
    #强制要求升级 audio-separator 以识别新模型
    required = ["pydub", "librosa", "numpy", "faster-whisper", "audio-separator[cpu]"] 
    need_install = []
    for lib in required:
        check_name = lib.split('[')[0].replace("-", "_")
        try:
            __import__(check_name)
        except ImportError:
            need_install.append(lib)

    if need_install:
        print(f">> [Python] 正在安装/升级核心组件: {need_install}")
        subprocess.check_call([sys.executable, "-m", "pip", "install", "--upgrade"] + need_install + 
                             ["-i", "https://pypi.tuna.tsinghua.edu.cn/simple", "--no-warn-script-location"])

install_dependencies()

from pydub import silence, AudioSegment
from faster_whisper import WhisperModel
try:
    from audio_separator.separator import Separator
except ImportError:
    Separator = None

#显式指定给 pydub
if os.path.exists(ffmpeg_bin):
    AudioSegment.converter = os.path.join(ffmpeg_bin, "ffmpeg.exe")
    AudioSegment.ffprobe = os.path.join(ffmpeg_bin, "ffprobe.exe")

def process_pipeline(args):
    input_path = args.input
    output_dir = args.output
    if not os.path.exists(output_dir): os.makedirs(output_dir)

    lang_map = {
            "中文": "zh",
            "日本語": "ja",
            "English": "en",
            "한국어": "ko"
        }
    target_lang = lang_map.get(args.language, "zh")

    # --- 步骤 1/4: UVR5 ---
    if args.uvr5 and Separator:
        print(">> [步骤 1/4] 正在执行 UVR5 人声分离...")
        try:
            #强制指定模型目录，避免路径冲突
            m_dir = os.path.join(runtime_dir, "models", "uvr5")
            os.makedirs(m_dir, exist_ok=True)
            separator = Separator(model_file_dir=m_dir)
            separator.load_model("UVR-MDX-NET-Voc_FT.onnx")
            output_files = separator.separate(input_path)
            vocal_file = next((f for f in output_files if "Vocals" in f), None)
            if vocal_file:
                input_path = os.path.join(os.getcwd(), vocal_file)
                print(f">> [成功] 已提取纯净人声: {vocal_file}")
        except Exception as e:
            print(f">> [警告] UVR5 失败 (将使用原音频): {str(e)}")

        print("\n>> [任务进度] UVR5 预处理已完成，准备进入 ASR 文本识别阶段。")
        print(">> [提示] 正在加载模型并转写音频... 这可能需要 1-3 分钟（取决于模型规模与 CPU 性能）。")
        print(">> [提示] 控制台可能会有一段时间没有输出，请耐心等待。")

    # --- 步骤 2/4: 加载 ASR ---
    print(f">> [步骤 2/4] 正在加载 ASR 模型 ({args.model_size})...")
    try:
        device = "cuda" if args.use_cuda else "cpu"
        #CPU 模式下强制使用 int8 保证低配机器也能跑
        model = WhisperModel(args.model_size, device=device, compute_type="int8")
        audio = AudioSegment.from_file(input_path)
    except Exception as e:
        print(f">> [致命错误] ASR 初始化失败: {str(e)}")
        return False

    # --- 步骤 3/4: 切片 ---
    print(">> [步骤 3/4] 正在执行自动切片...")
    chunks = silence.split_on_silence(audio, min_silence_len=700, silence_thresh=-40, keep_silence=300)
    
    # --- 步骤 4/4: 识别 ---
    label_lines = []
    print(f">> [步骤 4/4] 正在以 {args.language} ({target_lang}) 模式识别切片... (共 {len(chunks)} 条候选)")
    
    for i, chunk in enumerate(chunks):
        if len(chunk) < 2000: continue 
        file_name = f"slice_{i:04d}.wav"
        save_path = os.path.join(output_dir, file_name)
        chunk.export(save_path, format="wav")
        
        try:
            segments, _ = model.transcribe(save_path, beam_size=5, language=target_lang, initial_prompt=args.initial_prompt)
            text = "".join([s.text for s in segments]).strip()
            #只有识别成功才会在 console 打印预览
            if text:
                print(f"   [OK] {file_name} -> {text}")
            else:
                text = "未识别出文字"
        except Exception as e:
            #将具体报错写进 list.txt，方便调试
            text = f"识别出错: {str(e)[:30]}" 
            print(f"   [Error] {file_name} 识别崩溃: {e}")

        label_lines.append(f"{os.path.abspath(save_path)}|Yachi|{target_lang}|{text}")

    with open(os.path.join(output_dir, "list.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(label_lines))

    print(f">> [成功] 全链路处理完成！")
    return True

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--uvr5", action="store_true")
    parser.add_argument("--model_size", default="small")
    parser.add_argument("--use_cuda", action="store_true")
    parser.add_argument("--language", default="中文")
    parser.add_argument("--initial_prompt", default="")
    args = parser.parse_args()
    success = process_pipeline(args)
    sys.exit(0 if success else 1)