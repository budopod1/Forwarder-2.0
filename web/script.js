const URL_SHOW_AMOUNT = 25;
const NEW_TAB_URL = "/forwarder/new_tab"

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

function updateTabName(tab) {
    tab.tabTxt.innerText = tab.name;
}

function getTabOrigin(tab) {
    if (tab.url == NEW_TAB_URL) return null;
    return tab.origin;
}

function urlPath(url) {
    return url.pathname + url.search + url.hash
}

function displayTabURL(tab, url) {
    tab.url = urlPath(url);
    let origin = getTabOrigin(tab);
    tab.displayedURL = origin == null ? "" : origin + tab.url;
    if (tab.showing)
        urlBarInput.value = tab.displayedURL;
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
        await navigateToPage(tab, `https://www.youtube-nocookie.com/embed/${params.get("v")}`);
        return true;
    }
    return false;
}

function proccessLink(tab, a) {
    let target = a.getAttribute("target");
    if (target != "_blank") {
        a.removeAttribute("target");
    }
    let href = a.getAttribute("href");
    if (href != null && URL.canParse(href)) {
        let hrefURL = new URL(href);
        if (tab.origin == hrefURL.origin) {
            a.setAttribute("href", urlPath(hrefURL));
        } else {
            a.setAttribute("href", "#");
            a.addEventListener("click", () => 
                navigateToPage(tab, hrefURL.toString())
            );
        }
    }
    if (a.id == "forwarder-2-redirect") {
        a.click();
    }
}

async function iframeHandler(tab, iframe) {
    if (iframe.contentDocument == null) return;
    let idocument = iframe.contentWindow.document;
    let ilocation = iframe.contentWindow.location;

    let bases = idocument.getElementsByTagName("base");
    for (let base of bases) {
        base.remove();
    }

    let as = idocument.getElementsByTagName("a");
    for (let a of as) {
        proccessLink(tab, a);
    }

    await displayTabURL(tab, ilocation);
    if (await specialURLRedirect(tab)) {
        return;
    }

    let title = idocument.querySelector("title");
    tab.name = title == null ? tab.displayedURL : title.innerText;
    if (tab.name.length > URL_SHOW_AMOUNT) {
        tab.name = tab.name.slice(0, URL_SHOW_AMOUNT).trimEnd() + "...";
    }
    updateTabName(tab);
}

async function addNewTab() {
    let section = document.createElement("section");
    let iframe = document.createElement("iframe");
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
        url, displayedURL: "", origin: null, section, iframe, tabBtn, name: "New Tab",
        showing: false, tabTxt
    };
    closeBtn.onclick = (e) => {
        e.stopPropagation();
        closeTab(tab);
    };
    tabs.push(tab);
    updateTabName(tab);
    tabBtn.onclick = () => switchTab(tab);
    await switchTab(tab);
    
    iframe.addEventListener("load", () => {
        iframe.addEventListener("load", () => {
            iframeHandler(tab, iframe);
        });
        iframe.src = THIS_ORIGIN + url;
    }, {once: true});
    section.appendChild(iframe);
    mainElem.appendChild(section);
}

async function navigateToPage(tab, urlTxt) {
    let slashIndex = urlTxt.indexOf("://");
    if (slashIndex == -1) {
        urlTxt = "https://" + urlTxt;
    }
    let url = new URL(urlTxt);
    let dotIndex = urlTxt.indexOf(".");
    if (dotIndex == -1) {
        url.hostname += ".com";
    }
    tab.origin = url.origin;
    if (await setTabURL(tab, url)) {
        tab.iframe.src = tab.url;
    }
}

onload = () => {
    mainElem = document.querySelector("main");
    tabsElem = document.getElementById("tabs");
    urlBarInput = document.getElementById("url-bar");
    newTabBtn = document.getElementById("new-tab-btn");
    settingsModal = document.getElementById("settings-modal");
    pageTitleInput = document.getElementById("page-title");
    faviconLocationInput = document.getElementById("favicon-location");
    
    addNewTab();
    newTabBtn.onclick = addNewTab;

    document.getElementById("back-btn").onclick = () => {
        currentTab.iframe.contentWindow.history.back();
    };

    document.getElementById("forward-btn").onclick = () => {
        currentTab.iframe.contentWindow.history.forward();
    };

    document.getElementById("reload-btn").onclick = () => {
        currentTab.iframe.contentWindow.location.reload();
    };

    document.getElementById("url-bar-holder").onsubmit = (e) => {
        e.preventDefault();
        navigateToPage(currentTab, urlBarInput.value);
    };

    document.getElementById("settings-btn").onclick = () => {
        settingsModal.style.display = "flex";
    };

    document.getElementById("settings-modal-close").onclick = () => {
        settingsModal.style.display = "none";
    };

    pageTitleInput.onchange = () => {
        let pageTitle = pageTitleInput.value;
        localStorage.setItem("title", pageTitle);
        setTitle(pageTitle);
    };

    faviconLocationInput.onchange = () => {
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
    };

    {
        let pageTitle = localStorage.getItem("title");
        if (pageTitle != null) setTitle(pageTitle);

        setFavicon(localStorage.getItem("favicon"));
    }
};
