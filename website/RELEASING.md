# Releasing the website

https://odbc.postgresql.org/ is served by CloudFront from an S3 bucket. The `Deploy Website` workflow (`.github/workflows/docs-deploy.yml`) builds the site in `website/` and uploads it. It deploys only from the `www-release` branch, so a change merged to `main` is not published until someone releases it.

Release the website after merging changes under `website/`, and after each driver release, because the changelog page is built from `Changelog.md`.

## Steps

1. **Merge the changes to `main`.** On a pull request, the `docs` workflow builds the site and lints the two workflows. It runs when the pull request changes `website/`, `Changelog.md` or `docs/`.

2. **Point `www-release` at the commit to publish,** normally the tip of `main`:

   ```sh
   git fetch origin
   git push origin origin/main:refs/heads/www-release
   ```

   The push fails if `www-release` has commits that `main` doesn't. That only happens after a rollback, described below; use `--force` then.

3. **Start the deploy and wait for it:**

   ```sh
   gh workflow run docs-deploy.yml -R postgresql-interfaces/psqlodbc --ref www-release
   gh run watch -R postgresql-interfaces/psqlodbc \
     "$(gh run list -R postgresql-interfaces/psqlodbc --workflow docs-deploy.yml --branch www-release --limit 1 --json databaseId --jq '.[0].databaseId')"
   ```

4. **Check the live site.** https://odbc.postgresql.org/deploy-info.json shows the commit, the driver version from `configure.ac`, the time and the run ID of the deploy that is live:

   ```sh
   curl -s https://odbc.postgresql.org/deploy-info.json
   ```

   CloudFront's cache is cleared at the end of each deploy, so the new pages show straight away.

## What a deploy does

1. Checks that the run is on `www-release`. A run on any other branch fails before it builds anything.
2. Builds the site with the Hugo version that `.github/workflows/docs.yml` pins.
3. Uploads the site to the root of the bucket.
4. Deletes the files in the bucket that are no longer in the site, after the upload, so that a page that moved stays available while the new files upload.
5. Uploads `/deploy-info.json`.
6. Invalidates the CloudFront cache.

Each deploy replaces the previous one, and the bucket keeps no earlier version.

## Rolling back

Point `www-release` at an earlier commit and deploy again:

```sh
git push --force origin <commit>:refs/heads/www-release
gh workflow run docs-deploy.yml -R postgresql-interfaces/psqlodbc --ref www-release
```

## Infrastructure

PostgreSQL infrastructure owns the AWS account that hosts the site, and the `postgresql.org` DNS. The workflow signs in to AWS through GitHub OIDC, with no stored keys. It reads the role, region, bucket and distribution from repository variables, listed at the top of `.github/workflows/docs-deploy.yml`.

If the deploy fails at "Configure AWS credentials" with `Not authorized to perform sts:AssumeRoleWithWebIdentity`, the role's trust policy does not accept the token. The token's subject is `repo:postgresql-interfaces/psqlodbc:ref:refs/heads/www-release` and its audience is `sts.amazonaws.com`. Ask PostgreSQL infrastructure to check the trust policy.
