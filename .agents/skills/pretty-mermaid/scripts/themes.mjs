#!/usr/bin/env node

async function loadBeautifulMermaid() {
  try {
    return await import('beautiful-mermaid');
  } catch (e) {
    console.error('[beautiful-mermaid] Dependency not installed.');
    console.error('After approval, run `npm ci` in .agents/skills/pretty-mermaid.');
    console.error(`Original error: ${e.message}`);
    process.exit(1);
  }
}

async function main() {
  const { THEMES } = await loadBeautifulMermaid();
  const themes = Object.keys(THEMES);

  console.log('Available Beautiful-Mermaid Themes:\n');
  themes.forEach((theme, i) => {
    console.log(`${String(i + 1).padStart(2)}. ${theme}`);
  });

  console.log(`\nTotal: ${themes.length} themes`);
  console.log('\nUsage:');
  console.log('  node scripts/render.mjs --input diagram.mmd --theme <theme-name> --output output.svg');
}

main().catch(e => {
  console.error('Error:', e.message);
  process.exit(1);
});
