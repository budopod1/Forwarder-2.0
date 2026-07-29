(() => {
    let messageCallbacks = new Map();

    let idCounter = 0;

    function urlPath(url) {
        return url.pathname + url.search + url.hash
    }

    addEventListener("message", (e) => {
        if (!e.data.forwarder2) return;
        messageCallbacks.get(e.data.completion).resolve(e.data.result);
    });

    async function rpc(func, async_=false, ...args) {
        let id = idCounter++;
        let {promise, resolve} = Promise.withResolvers();
        messageCallbacks.set(id, {resolve, promise});
        top.postMessage({forwarder2: true, func, async_, args, completion: id});
        let result = await promise;
        messageCallbacks.delete(id);
        return result;
    }
    
    async function setOrigin(origin) {
        return await rpc("setOrigin", async_=true, origin);
    }

    function urlPath(url) {
        return url.pathname + url.search + url.hash
    }

    async function navigateToURL(trueOrigin, url) {
        if (trueOrigin != url.origin) await setOrigin(url.origin);
        let proxiedURL = new URL(url);
        proxiedURL.protocol = location.protocol;
        proxiedURL.host = location.host;
        location.href = proxiedURL.toString();
    }

    function processElement(elem, trueOrigin, attr, onNewOrigin) {
        let href = elem.getAttribute(attr);
        if (href != null && href != "") {
            if (href.startsWith("//")) href = "https:" + href;
            if (URL.canParse(href)) {
                let url = new URL(href);
                if (trueOrigin == url.origin) {
                    elem.setAttribute(attr, urlPath(url));
                } else {
                    onNewOrigin(url);
                }
            }
        }
    }

    function processLink(trueOrigin, link) {
        link.removeAttribute("target");

        processElement(link, trueOrigin, "href", (url) => {
            link.setAttribute("href", "#");
            link.addEventListener("click", () => 
                navigateToURL(trueOrigin, url)
            );
        });
    }

    function processForm(trueOrigin, form) {
        processElement(form, trueOrigin, "action", (url) => {
            form.setAttribute("action", urlPath(url));
            form.addEventListener("submit", (e) => {
                e.preventDefault();
                (async () => {
                    await setOrigin(url.origin);
                    form.submit();
                })();
            });
        });
    }

    let getTrueOrigin = rpc("getCurrentOrigin", async_=false);

    async function processSubTree(tree) {
        let trueOrigin = await getTrueOrigin;

        for (let base of tree.getElementsByTagName("base")) {
            processLink(trueOrigin, base);
        }

        for (let a of tree.getElementsByTagName("a")) {
            processLink(trueOrigin, a);
        }

        for (let form of tree.getElementsByTagName("form")) {
            processForm(trueOrigin, form);
        }
    }

    addEventListener("DOMContentLoaded", () => (async () => {
        if (window.parent == window.top) {
            let title = document.querySelector("title");
            rpc("updateCurrentTabTitle", async_=false, title?.innerText);
        }

        new MutationObserver((mutationList, _) => (async () => {
            for (let mutation of mutationList) {
                if (mutation.type == "childList") {
                    for (let added of mutation.addedNodes) {
                        await processSubTree(added);
                    }
                } else if (mutation.type == "attributes") {
                    await processSubTree(mutation.target);
                }
            }
        })()).observe(document.body, {
            subtree: true, childList: true, attributes: true
        });

        await processSubTree(document.documentElement);

        rpc("displayCurrentTabPath", async_=false, urlPath(location));
    })());
})();
