// Shared helpers for the web frame tests.
import { execFileSync } from 'node:child_process';
import { mkdtempSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
export const webDir = join(here, '..', '..', 'web');

export function tempDir() {
  return mkdtempSync(join(tmpdir(), 'web-frame-test-'));
}

// Fills a template through CMake's configure_file, like the game builds do.
export function render(templateName, values, outDir, content) {
  const out = join(outDir, templateName.replace(/\.in$/, ''));
  const args = ['-DTEMPLATE=' + join(webDir, templateName), '-DOUT=' + out];
  for (const [name, value] of Object.entries(values)) args.push(`-D${name}=${value}`);
  if (content !== undefined) {
    const contentFile = join(outDir, 'site-content.html');
    writeFileSync(contentFile, content);
    args.push('-DSITE_CONTENT_FILE=' + contentFile);
  }
  args.push('-P', join(here, 'render.cmake'));
  execFileSync('cmake', args, { stdio: 'pipe' });
  return out;
}

// Any placeholder CMake would fill in but was given no value for.
export const leftoverPlaceholder = /@[A-Za-z_][A-Za-z0-9_]*@/;
