/* ============================================================
   FTXUI Website — Main JavaScript
   ============================================================ */

// --- Sticky Nav ---
(function () {
  const nav = document.querySelector('.glass-nav');
  if (!nav) return;
  const onScroll = () => {
    nav.classList.toggle('scrolled', window.scrollY > 50);
  };
  window.addEventListener('scroll', onScroll, { passive: true });
  onScroll();
})();

// --- Mobile Hamburger ---
(function () {
  const ham = document.querySelector('.hamburger');
  const links = document.querySelector('.nav-links');
  if (!ham || !links) return;
  ham.addEventListener('click', () => {
    links.classList.toggle('open');
    ham.setAttribute('aria-expanded', links.classList.contains('open'));
  });
  // Close on link click
  links.querySelectorAll('a').forEach(a => {
    a.addEventListener('click', () => links.classList.remove('open'));
  });
})();

// --- Scroll Animations (IntersectionObserver) ---
(function () {
  const els = document.querySelectorAll('.animate-on-scroll');
  if (!els.length) return;
  const obs = new IntersectionObserver((entries) => {
    entries.forEach(e => {
      if (e.isIntersecting) {
        e.target.classList.add('visible');
        obs.unobserve(e.target);
      }
    });
  }, { threshold: 0.1, rootMargin: '0px 0px -40px 0px' });
  els.forEach(el => obs.observe(el));
})();

// --- Tab Switcher ---
(function () {
  document.querySelectorAll('.tab-container').forEach(container => {
    const btns = container.querySelectorAll('.tab-btn');
    const panels = container.querySelectorAll('.tab-panel');
    btns.forEach((btn, i) => {
      btn.addEventListener('click', () => {
        btns.forEach(b => b.classList.remove('active'));
        panels.forEach(p => p.classList.remove('active'));
        btn.classList.add('active');
        panels[i] && panels[i].classList.add('active');
      });
    });
    // Activate first tab
    if (btns[0]) btns[0].classList.add('active');
    if (panels[0]) panels[0].classList.add('active');
  });
})();

// --- Copy Button for Code Blocks ---
(function () {
  document.querySelectorAll('.code-block').forEach(block => {
    if (block.querySelector('.copy-btn')) return;
    const btn = document.createElement('button');
    btn.className = 'copy-btn';
    btn.textContent = 'Copy';
    btn.setAttribute('aria-label', 'Copy code to clipboard');
    block.appendChild(btn);
    btn.addEventListener('click', () => {
      const code = block.querySelector('pre') || block;
      const text = code.innerText || code.textContent;
      navigator.clipboard.writeText(text).then(() => {
        btn.textContent = 'Copied!';
        btn.classList.add('copied');
        setTimeout(() => {
          btn.textContent = 'Copy';
          btn.classList.remove('copied');
        }, 2000);
      }).catch(() => {
        btn.textContent = 'Error';
        setTimeout(() => { btn.textContent = 'Copy'; }, 2000);
      });
    });
  });
})();

// --- Smooth Scroll Anchors ---
(function () {
  document.querySelectorAll('a[href^="#"]').forEach(a => {
    a.addEventListener('click', e => {
      const id = a.getAttribute('href').slice(1);
      const target = document.getElementById(id);
      if (target) {
        e.preventDefault();
        const offset = 80; // nav height
        const top = target.getBoundingClientRect().top + window.scrollY - offset;
        window.scrollTo({ top, behavior: 'smooth' });
      }
    });
  });
})();

// --- Hero Typewriter Effect ---
(function () {
  const output = document.getElementById('terminal-output');
  if (!output) return;

  const sessions = [
    {
      lines: [
        { text: '$ cmake -S . -B build && cmake --build build', delay: 60 },
        { text: '\n', delay: 0 },
        { text: '[  0%] Building CXX object ftxui/screen...\n', delay: 30 },
        { text: '[ 50%] Building CXX object ftxui/dom...\n', delay: 30 },
        { text: '[100%] Linking CXX executable myapp\n', delay: 30 },
        { text: '\n', delay: 0 },
        { text: '$ ./myapp\n', delay: 60 },
        { text: '\n', delay: 0 },
        { text: '╔══════════════════════════════════════╗\n', delay: 20 },
        { text: '║     ███████╗████████╗██╗  ██╗██╗    ║\n', delay: 20 },
        { text: '║     ██╔════╝╚══██╔══╝╚██╗██╔╝██║    ║\n', delay: 20 },
        { text: '║     █████╗     ██║    ╚███╔╝ ██║    ║\n', delay: 20 },
        { text: '║     ██╔══╝     ██║    ██╔██╗ ██║    ║\n', delay: 20 },
        { text: '║     ██║        ██║   ██╔╝ ██╗██║    ║\n', delay: 20 },
        { text: '║     ╚═╝        ╚═╝   ╚═╝  ╚═╝╚═╝    ║\n', delay: 20 },
        { text: '║   The C++ Terminal UI Framework     ║\n', delay: 20 },
        { text: '╚══════════════════════════════════════╝\n', delay: 20 },
      ]
    },
    {
      lines: [
        { text: '$ cat hello.cpp\n', delay: 60 },
        { text: '\n', delay: 0 },
        { text: '#include "ftxui/ui/all.hpp"\n', delay: 25 },
        { text: 'using namespace ftxui::ui;\n', delay: 25 },
        { text: '\n', delay: 0 },
        { text: 'int main() {\n', delay: 25 },
        { text: '  SetTheme(Theme::Nord());\n', delay: 25 },
        { text: '  auto app = Layout::Vertical({\n', delay: 25 },
        { text: '    Text("Hello from FTXUI!"),\n', delay: 25 },
        { text: '    LineChart().Series("cpu", data).Build(),\n', delay: 25 },
        { text: '    GeoMap().Data(WorldMapGeoJSON()).Build(),\n', delay: 25 },
        { text: '  });\n', delay: 25 },
        { text: '  RunFullscreen(app);\n', delay: 25 },
        { text: '}\n', delay: 25 },
        { text: '\n', delay: 0 },
        { text: '$ g++ hello.cpp -lftxui-ui && ./a.out\n', delay: 60 },
      ]
    },
    {
      lines: [
        { text: '$ ./globe_demo\n', delay: 60 },
        { text: '\n', delay: 0 },
        { text: '  ⣀⣀⠤⠤⠒⠒⠒⠂⠂⠂⠂⠂⠂⠒⠒⠒⠤⠤⣀⣀  \n', delay: 20 },
        { text: ' ⣀⠔⠁         ⠁⠔⣀ \n', delay: 20 },
        { text: '⡠⠁  ⡠⠤⠒⠒⠤⡀   ⡀⠒⠤⠁  ⠁⠢⡀\n', delay: 20 },
        { text: '⡇   ⡇  ⣤⡀⠸⡄  ⢸ ⣤   ⢸  ⡇\n', delay: 20 },
        { text: '⠱⡀  ⠱⣀⣀⣀⣀⠜   ⠓⣀⣀⠜  ⡠⠃\n', delay: 20 },
        { text: ' ⠑⠢⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⠔⠁ \n', delay: 20 },
        { text: '\n', delay: 0 },
        { text: '  🌍 Spinning 3D Braille Globe\n', delay: 30 },
        { text: '  📍 Cities: NYC, LON, TOK, SYD\n', delay: 30 },
        { text: '  ↻  Rotating at 20°/sec\n', delay: 30 },
        { text: '\n', delay: 0 },
        { text: '  [Q] Quit  [+/-] Speed  [Mouse] Pan\n', delay: 30 },
      ]
    }
  ];

  let sessionIndex = 0;
  let charBuffer = '';
  let typing = false;

  async function sleep(ms) {
    return new Promise(r => setTimeout(r, ms));
  }

  async function typeSession(session) {
    for (const line of session.lines) {
      for (const ch of line.text) {
        charBuffer += ch;
        output.textContent = charBuffer;
        if (line.delay > 0) await sleep(line.delay + Math.random() * 20);
      }
    }
  }

  async function runTypewriter() {
    while (true) {
      charBuffer = '';
      output.textContent = '';
      await typeSession(sessions[sessionIndex]);
      await sleep(3000);
      sessionIndex = (sessionIndex + 1) % sessions.length;
    }
  }

  // Start after a short delay
  setTimeout(runTypewriter, 600);
})();

// --- Active nav link highlighting ---
(function () {
  const links = document.querySelectorAll('.nav-link');
  const current = window.location.pathname.split('/').pop() || 'index.html';
  links.forEach(link => {
    const href = link.getAttribute('href').split('/').pop();
    if (href === current || (current === '' && href === 'index.html')) {
      link.classList.add('active');
    }
  });
})();

// --- Lazy number counter animation ---
(function () {
  const counters = document.querySelectorAll('[data-count]');
  if (!counters.length) return;
  const obs = new IntersectionObserver((entries) => {
    entries.forEach(e => {
      if (!e.isIntersecting) return;
      const el = e.target;
      const target = parseFloat(el.dataset.count);
      const suffix = el.dataset.suffix || '';
      const prefix = el.dataset.prefix || '';
      const duration = 1500;
      const start = performance.now();
      const isFloat = String(target).includes('.');
      function update(now) {
        const elapsed = now - start;
        const progress = Math.min(elapsed / duration, 1);
        const ease = 1 - Math.pow(1 - progress, 3);
        const val = target * ease;
        el.textContent = prefix + (isFloat ? val.toFixed(1) : Math.round(val)) + suffix;
        if (progress < 1) requestAnimationFrame(update);
        else el.textContent = prefix + (isFloat ? target.toFixed(1) : target) + suffix;
      }
      requestAnimationFrame(update);
      obs.unobserve(el);
    });
  }, { threshold: 0.5 });
  counters.forEach(el => obs.observe(el));
})();
