const URL_SHOW_AMOUNT = 25;
const NEW_TAB_URL = "/forwarder/new_tab";
const URL_REGEX = /^(\w+:\/\/)?([A-Za-z0-9_\-]+\.)+\w{2,}(\/[A-Za-z0-9\-._~:?#[\]@!$&'()*+,;%=]*)*$/;

let THIS_ORIGIN = location.origin;

let mainElem;
let tabsElem;
let newTabBtn;
let urlBarInput;
let settingsModal;
let pageTitleInput;
let faviconLocationInput;

let tabs = [];
let currentTab = null;
let currentOrigin = null;

function setTitle(title) {
    document.querySelector("title").innerText = title;
    pageTitleInput.value = title;
}

function setFavicon(href) {
    if (href == null) href = "";
    localStorage.setItem("favicon", href);
    document.getElementById("favicon").href = 
        href == "" ? "/forwarder/favicon.ico" : href;
    faviconLocationInput.value = href;
}

async function setOrigin(origin) {
    if (origin == null || currentOrigin == origin) return true;
    let response = await fetch(THIS_ORIGIN + "/forwarder/change_origin", {
        "method": "POST", "body": origin
    });
    if (response.status == 200) {
        currentOrigin = origin;
        return true;
    } else if (response.status == 403) {
        alert("Sorry, but the website you requested, "+origin+", hasn't been verified for use with Forwarder 2.0. Please contact the Forwarder developers to have it verfied and allowlisted.");
        return false;
    } else {
        console.error("Unexpected status code when setting origin, " + response.status);
        return false;
    }
}

async function switchTab(tab) {
    if (currentTab == tab) return;
    currentTab = tab;
    for (let other of tabs) {
        if (other == tab) continue;
        other.showing = false;
        other.tabBtn.classList.remove("current-tab");
        other.section.style.display = "none";
    }
    tab.showing = true;
    await setOrigin(tab.origin);
    tab.tabBtn.classList.add("current-tab");
    tab.section.style.display = "block";
    urlBarInput.value = tab.displayedURL;
}

function displayTabName(tab) {
    tab.tabTxt.innerText = tab.name;
}

function updateCurrentTabTitle(txt) {
    let tab = currentTab;
    txt = txt == null ? tab.url : txt;
    if (txt.length > URL_SHOW_AMOUNT)
        txt = txt.slice(0, URL_SHOW_AMOUNT).trimEnd() + "...";
    tab.name = txt;
    displayTabName(tab, txt);
}

function getCurrentOrigin() {
    return currentTab.origin;
}

function urlPath(url) {
    return url.pathname + url.search + url.hash
}

function displayTabURL(tab, url) {
    tab.url = urlPath(url);
    tab.displayedURL = tab.origin == null ? "" : tab.origin + tab.url;
    if (tab.showing)
        urlBarInput.value = tab.displayedURL;
}

function displayCurrentTabPath(path) {
    displayTabURL(currentTab, new URL(currentTab.url.origin + path));
}

async function setTabURL(tab, url) {
    if (!await setOrigin(tab.origin)) return false;
    displayTabURL(tab, url);
    return true;
}

async function closeTab(tab) {
    tab.section.remove();
    tab.tabBtn.remove();
    tabs = tabs.filter(other => other != tab);
    if (tab == currentTab) {
        if (tabs.length == 0) {
            await addNewTab();
        } else {
            await switchTab(tabs.at(-1));
        }
    }
}

async function specialURLRedirect(tab) {
    if (tab.origin == null) return false;
    if (tab.origin.includes("youtube.com")
        && tab.url.startsWith("/watch")) {
        let params = new URLSearchParams(location.search);
        await navigateToURL(tab, new URL(`https://www.youtube-nocookie.com/embed/${params.get("v")}`));
        return true;
    }
    return false;
}

async function addNewTab() {
    let section = document.createElement("section");
    let iframe = document.createElement("iframe");
    iframe.sandbox = "allow-downloads allow-forms allow-modals allow-orientation-lock allow-pointer-lock allow-popups allow-popups-to-escape-sandbox allow-presentation allow-same-origin allow-scripts allow-storage-access-by-user-activation";
    let url = NEW_TAB_URL;

    let tabBtn = document.createElement("button");
    tabBtn.classList.add("tab");
    
    let tabTxt = document.createElement("span");
    tabBtn.appendChild(tabTxt);
    
    let closeBtn = document.createElement("button")
    closeBtn.classList.add("close-tab-btn");
    closeBtn.innerText = "🞨";
    tabBtn.appendChild(closeBtn);
    
    tabsElem.insertBefore(tabBtn, newTabBtn);

    let tab = {
        url, displayedURL: "", origin: null, section, iframe, tabBtn,
        name: "New Tab", showing: false, tabTxt, hist: []
    };
    closeBtn.addEventListener("click", (e) => {
        e.stopPropagation();
        closeTab(tab);
    });
    tabs.push(tab);
    displayTabName(tab);
    tabBtn.addEventListener("click", () => switchTab(tab));
    await switchTab(tab);

    iframe.addEventListener("load", () => {
        iframe.src = THIS_ORIGIN + url;
    }, {once: true});

    section.appendChild(iframe);
    mainElem.appendChild(section);
}

async function navigateToURL(tab, url) {
    tab.origin = url.origin;
    if (await setTabURL(tab, url)) {
        tab.iframe.src = tab.url;
    }
}

async function redirectCurrentTab(txt) {
    if (txt.startsWith("/")) {
        txt = currentOrigin + txt;
    }
    await navigateToURL(currentTab, new URL(txt));
}

async function navigateGivenInput(tab, input) {
    let url;
    if (input.match(URL_REGEX)) {
        if (input.indexOf("://") == -1) {
            input = "https://" + input;
        }
        url = new URL(input);
    } else {
        url = new URL(`https://duckduckgo.com/?q=${encodeURIComponent(input)}`);
    }
    await navigateToURL(tab, url);
}

addEventListener("load", () => {
    mainElem = document.querySelector("main");
    tabsElem = document.getElementById("tabs");
    urlBarInput = document.getElementById("url-bar");
    newTabBtn = document.getElementById("new-tab-btn");
    settingsModal = document.getElementById("settings-modal");
    pageTitleInput = document.getElementById("page-title");
    faviconLocationInput = document.getElementById("favicon-location");
    
    addNewTab();
    newTabBtn.addEventListener("click", addNewTab);

    document.getElementById("back-btn").addEventListener("click", () => {
        currentTab.iframe.contentWindow.history.back();
    });

    document.getElementById("forward-btn").addEventListener("click", () => {
        currentTab.iframe.contentWindow.history.forward();
    });

    document.getElementById("reload-btn").addEventListener("click", () => {
        currentTab.iframe.contentWindow.location.reload();
    });

    document.getElementById("url-bar-holder").addEventListener("submit", (e) => {
        e.preventDefault();
        navigateGivenInput(currentTab, urlBarInput.value);
    });

    document.getElementById("settings-btn").addEventListener("click", () => {
        settingsModal.style.display = "flex";
    });

    document.getElementById("settings-modal-close").addEventListener("click", () => {
        settingsModal.style.display = "none";
    });

    pageTitleInput.addEventListener("change", () => {
        let pageTitle = pageTitleInput.value;
        localStorage.setItem("title", pageTitle);
        setTitle(pageTitle);
    });

    faviconLocationInput.addEventListener("change", () => {
        let faviconHREF = faviconLocationInput.value;
        if (faviconHREF != "") {
            if (faviconHREF.match(/^\w+$/))
                faviconHREF += ".com";
            if (!faviconHREF.endsWith(".ico")) {
                if (!faviconHREF.endsWith("/")) 
                    faviconHREF += "/";
                faviconHREF += "favicon.ico";
            }
            if (!faviconHREF.startsWith("http")) 
                faviconHREF = "https://" + faviconHREF;
        }
        setFavicon(faviconHREF);
    });

    {
        let pageTitle = localStorage.getItem("title");
        if (pageTitle != null) setTitle(pageTitle);

        setFavicon(localStorage.getItem("favicon"));
    }
});

addEventListener("message", (e) => (async () => {
    if (!e.data.forwarder2) return;
    let result = window[e.data.func](...e.data.args);
    if (e.data.async_) result = await result;
    e.source.postMessage({forwarder2: true, completion: e.data.completion, result});
})());
