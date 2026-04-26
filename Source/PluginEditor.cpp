#include "PluginEditor.h"
#include <filesystem>
#include <fstream>
#include <sstream>

//==============================================================================
// PARAMETER ID MAPPING — React ID → JUCE ID
static const std::pair<const char*, const char*> kParamMap[] =
{
    { "inputTrim",      "inTrimDb"      },
    { "inputHighPass",  "inHPFHz"       },
    { "inputLowPass",   "inLPFHz"       },
    { "inputMono",      "inMono"        },
    { "inputPolarity",  "inPolarity"    },
    { "inputVoice",     "inVoice"       },
    { "inputBias",      "inBias"        },
    { "dynOn",          "compOn"        },
    { "dynSolo",        "compSolo"      },
    { "dynDelta",       "compDelta"     },
    { "scHpf",          "compSC"        },
    { "thresh",         "compThresh"    },
    { "dynRatio",       "compRatio"     },
    { "attack",         "compAttack"    },
    { "release",        "compRelease"   },
    { "stereoLink",     "compStereo"    },
    { "msMode",         "compMS"        },
    { "compressionType","compType"      },
    { "makeup",         "compMakeup"    },
    { "satOn",          "satOn"         },
    { "satSolo",        "satSolo"       },
    { "satDelta",       "satDelta"      },
    { "satMix",         "satMix"        },
    { "lowSplit",       "x1Hz"          },
    { "midSplit",       "x2Hz"          },
    { "highSplit",      "x3Hz"          },
    { "lowOn",          "b0_on"         },
    { "lowSolo",        "b0_solo"       },
    { "lowDelta",       "b0_delta"      },
    { "lowDrive",       "b0_driveDb"    },
    { "lowMix",         "b0_mix"        },
    { "lowMsFocus",     "b0_msFocus"    },
    { "lowAlgorithm",   "b0_algo"       },
    { "loMidOn",        "b1_on"         },
    { "loMidSolo",      "b1_solo"       },
    { "loMidDelta",     "b1_delta"      },
    { "loMidDrive",     "b1_driveDb"    },
    { "loMidMix",       "b1_mix"        },
    { "loMidMsFocus",   "b1_msFocus"    },
    { "loMidAlgorithm", "b1_algo"       },
    { "hiMidOn",        "b2_on"         },
    { "hiMidSolo",      "b2_solo"       },
    { "hiMidDelta",     "b2_delta"      },
    { "hiMidDrive",     "b2_driveDb"    },
    { "hiMidMix",       "b2_mix"        },
    { "hiMidMsFocus",   "b2_msFocus"    },
    { "hiMidAlgorithm", "b2_algo"       },
    { "highOn",         "b3_on"         },
    { "highSolo",       "b3_solo"       },
    { "highDelta",      "b3_delta"      },
    { "highDrive",      "b3_driveDb"    },
    { "highMix",        "b3_mix"        },
    { "highMsFocus",    "b3_msFocus"    },
    { "highAlgorithm",  "b3_algo"       },
    { "limOn",          "limitOn"       },
    { "limSolo",        "limitSolo"     },
    { "limDelta",       "limitDelta"    },
    { "limThreshold",   "limitThresh"   },
    { "limGain",        "limitGain"     },
    { "limAttack",      "limitAttack"   },
    { "ceiling",        "limitCeiling"  },
    { "limRelease",     "limitRelease"  },
    { "masterTrim",     "masterMTrim"   },
    { "harmonics",      "masterHarmonics"},
    { "shape",          "masterShape"   },
    { "depth",          "masterDepth"   },
    { "globalMix",      "masterMix"     },
    { "outputTrim",     "masterOutTrim" },
    { "masterOn",       "masterOn"      },
    { "masterSolo",     "masterSolo"    },
    { "masterDelta",    "masterDelta"   },
    { "clipOn",         "clipOn"        },
    { "clipSolo",       "clipSolo"      },
    { "clipDelta",      "clipDelta"     },
    { "drive",          "clipDrive"     },
    { "softness",       "clipSoftness"  },
    { "link",           "clipLink"      },
    { "hardBypass",     "bypass"        },
    { "delta",          "deltaGlob"     },
    { "oversample",     "oversample"    },
};

static juce::String juceIDForReactID (const juce::String& reactId)
{
    for (auto& [r, j] : kParamMap)
        if (reactId == r) return j;
    return {};
}

static juce::String reactIDForJuceID (const juce::String& juceId)
{
    for (auto& [r, j] : kParamMap)
        if (juceId == j) return r;
    return {};
}

//==============================================================================
// Find WebUI directory
static juce::File findWebUIDir()
{
    juce::File dev ("/Users/invisible/flowform-plugin/WebUI");
    if (dev.isDirectory()) return dev;

    auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
    auto dir = exe.getParentDirectory();
    for (int i = 0; i < 6; ++i)
    {
        auto candidate = dir.getChildFile ("WebUI");
        if (candidate.isDirectory()) return candidate;
        dir = dir.getParentDirectory();
    }
    return {};
}

static juce::String mimeForExtension (const juce::String& ext)
{
    if (ext == ".html") return "text/html";
    if (ext == ".css")  return "text/css";
    if (ext == ".js")   return "application/javascript";
    if (ext == ".json") return "application/json";
    if (ext == ".png")  return "image/png";
    if (ext == ".svg")  return "image/svg+xml";
    if (ext == ".ico")  return "image/x-icon";
    if (ext == ".ttf")  return "font/ttf";
    if (ext == ".woff") return "font/woff";
    if (ext == ".woff2") return "font/woff2";
    return "text/plain";
}

//==============================================================================
FlowFormAudioProcessorEditor::FlowFormAudioProcessorEditor (FlowFormAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),
      webView (juce::WebBrowserComponent::Options{}
          .withResourceProvider ([this] (const juce::String& url) -> std::optional<juce::WebBrowserComponent::Resource>
          {
              return getResource (url);
          }, juce::WebBrowserComponent::getResourceProviderRoot())
          .withNativeIntegrationEnabled())
{
    setSize (1200, 800);
    addAndMakeVisible (webView);

    auto& params = audioProcessor.getAPVTS();
    for (auto& [reactId, juceId] : kParamMap)
        params.addParameterListener (juceId, this);

    loadUI();
    startTimerHz (30);
}

FlowFormAudioProcessorEditor::~FlowFormAudioProcessorEditor()
{
    stopTimer();
    auto& params = audioProcessor.getAPVTS();
    for (auto& [reactId, juceId] : kParamMap)
        params.removeParameterListener (juceId, this);
}

void FlowFormAudioProcessorEditor::resized()
{
    webView.setBounds (getLocalBounds());
}

void FlowFormAudioProcessorEditor::loadUI()
{
    auto uiDir = findWebUIDir();
    if (uiDir.isDirectory())
    {
        auto html = uiDir.getChildFile ("index.html");
        if (html.existsAsFile())
        {
            // Load via resource provider root so asset requests get intercepted
            auto url = juce::WebBrowserComponent::getResourceProviderRoot() + "index.html";
            webView.goToURL (url);
            uiLoaded = true;
            DBG ("[FlowForm] WebUI loaded: " + url);
            return;
        }
        else
        {
            DBG ("[FlowForm] index.html not found in " + uiDir.getFullPathName());
        }
    }
    else
    {
        DBG ("[FlowForm] WebUI dir not found");
    }
}

//==============================================================================
// Resource provider — serves WebUI assets + handles parameter changes from React
std::optional<juce::WebBrowserComponent::Resource> FlowFormAudioProcessorEditor::getResource (const juce::String& url)
{
    // Handle parameter changes from React via iframe URL navigation
    if (url.startsWith ("juce://param/"))
    {
        auto parts = juce::StringArray::fromTokens (url.substring (13), "/", "");
        if (parts.size() >= 2)
        {
            auto reactId = parts[0];
            auto value   = parts[1].getFloatValue();
            auto juceId  = juceIDForReactID (reactId);
            if (juceId.isNotEmpty())
            {
                if (auto* param = audioProcessor.getAPVTS().getParameter (juceId))
                {
                    auto normalised = param->convertTo0to1 (value);
                    param->setValueNotifyingHost (normalised);
                }
            }
        }
        return std::nullopt;
    }

    auto uiDir = findWebUIDir();
    if (!uiDir.isDirectory())
        return std::nullopt;

    // URL comes as absolute path (e.g. /assets/index-xxx.js)
    // Strip leading slash and resolve relative to WebUI dir
    auto cleanUrl = url.trimCharactersAtStart ("/");
    // Remove file:// prefix if present
    if (cleanUrl.startsWithIgnoreCase ("file://"))
        cleanUrl = cleanUrl.fromFirstOccurrenceOf ("file://", false, false);

    auto file = uiDir.getChildFile (cleanUrl);
    if (!file.existsAsFile())
        return std::nullopt;

    juce::WebBrowserComponent::Resource res;

    auto stream = file.createInputStream();
    if (!stream || stream->getTotalLength() <= 0)
        return std::nullopt;

    auto size = (size_t) stream->getTotalLength();
    res.data.resize (size);
    stream->read (res.data.data(), (ssize_t) size);
    res.mimeType = mimeForExtension (file.getFileExtension());

    return res;
}

//==============================================================================
void FlowFormAudioProcessorEditor::sendToUI (const juce::String& json)
{
    if (!uiLoaded) return;
    webView.evaluateJavascript ("window.postMessage(" + json + ", '*')");
}

//==============================================================================
void FlowFormAudioProcessorEditor::parameterChanged (const juce::String& juceId, float newValue)
{
    auto reactId = reactIDForJuceID (juceId);
    if (reactId.isEmpty()) return;

    auto json = "{\"type\":\"parameter-update\",\"data\":{\"id\":\"" + reactId
                + "\",\"value\":" + juce::String (newValue, 4) + "}}";

    juce::MessageManager::callAsync ([this, json] { sendToUI (json); });
}

//==============================================================================
void FlowFormAudioProcessorEditor::timerCallback()
{
    if (!uiLoaded) return;

    auto sendMeter = [this] (const char* id, float val)
    {
        auto json = "{\"type\":\"meter-update\",\"data\":{\"id\":\""
            + juce::String(id) + "\",\"value\":" + juce::String (val, 4) + "}}";
        webView.evaluateJavascript ("window.postMessage(" + json + ", '*')");
    };

    sendMeter ("inputMeterL",    audioProcessor.getInLevelL());
    sendMeter ("inputMeterR",    audioProcessor.getInLevelR());
    sendMeter ("grMeter",        audioProcessor.getCompGR());
    sendMeter ("limGrMeter",     audioProcessor.getLimiterGR());
    sendMeter ("masterMeterL",   audioProcessor.getOutLevelL());
    sendMeter ("masterMeterR",   audioProcessor.getOutLevelR());
    sendMeter ("longTermLufs",   audioProcessor.getLufsIntegrated());
    sendMeter ("shortTermLufs",  audioProcessor.getLufsShortTerm());
    sendMeter ("integratedLufs", audioProcessor.getLufsMaxMomentary());
}
