static int cmd_bundle_unbundle(int argc, const char **argv, const char *prefix) {
	struct bundle_header header = BUNDLE_HEADER_INIT;
	int bundle_fd = -1;
	int ret;
	int progress = isatty(2);

	struct option options[] = {
		OPT_BOOL(0, "progress", &progress,
			 N_("show progress meter")),
		OPT_END()
	};
	char *bundle_file;
	struct strvec extra_index_pack_args = STRVEC_INIT;

	argc = parse_options_cmd_bundle(argc, argv, prefix,
			builtin_bundle_unbundle_usage, options, &bundle_file);
	/* bundle internals use argv[1] as further parameters */

	if ((bundle_fd = read_bundle_header(bundle_file, &header)) < 0) {
		ret = 1;
		goto cleanup;
	}
	if (!startup_info->have_repository)
		die(_("Need a repository to unbundle."));
	if (progress)
		strvec_pushl(&extra_index_pack_args, "-v", "--progress-title",
			     _("Unbundling objects"), NULL);
	ret = !!unbundle(the_repository, &header, bundle_fd,
			 &extra_index_pack_args) ||
		list_bundle_refs(&header, argc, argv);
	bundle_header_release(&header);
cleanup:
	free(bundle_file);
	return ret;
}
