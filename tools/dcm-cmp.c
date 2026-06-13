#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

#include <dicom/dicom.h>

static const char usage[] = "usage: dcm-cmp [-hVcl] FILE_PATH1 FILE_PATH2 ...";

int get_winsize(struct winsize *ws) { return ioctl(0, TIOCGWINSZ, ws); }

int read_dicom_and_error(const char *fname, DcmError **error,
                         DcmFilehandle **fhandle) {
  dcm_log_info("Read file '%s'", fname);
  *fhandle = dcm_filehandle_create_from_file(error, fname);
  if (fhandle == NULL) {
    dcm_error_print(*error);
    dcm_error_clear(error);
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

void cleanup_fhandles(DcmFilehandle *fhandle_1, DcmFilehandle *fhandle_2) {

  dcm_filehandle_destroy(fhandle_1);
  dcm_filehandle_destroy(fhandle_2);
}

int get_metadata_from_filehandle(DcmError **error, DcmFilehandle *fhandle,
                                 const DcmDataSet *metadata) {
  metadata = dcm_filehandle_get_metadata_subset(error, fhandle);
  if (metadata == NULL) {
    dcm_error_log(*error);
    dcm_error_clear(error);
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

bool print_metadata_element(const DcmElement *element, void *client) {
  DcmError *e1, *e2;
  const DcmDataSet *metadata_2 = (const DcmDataSet *)client;
  int curr_tag = dcm_element_get_tag(element);
  const char *val1, *val2;
  if (!dcm_element_get_value_string(&e1, element, 0, &val1)) {
    dcm_error_log(e1);
    dcm_error_clear(&e1);
    return false;
  }
  DcmElement *elem_2 = dcm_dataset_get(&e2, metadata_2, curr_tag);
  if (elem_2 == NULL || !dcm_element_get_value_string(&e2, elem_2, 0, &val2)) {
    dcm_error_log(e2);
    dcm_error_clear(&e2);
    return false;
  }
  const char *keyword = dcm_dict_keyword_from_tag(curr_tag);

  if (strcmp(val1, val2) != 0) {
    printf("%s\n", keyword);
    printf("%s %s\n", val1, val2);
  } else {
    printf("\033[43m%s\033[m\n", keyword);
    printf("\033[43m%s %s\033[m\n", val1, val2);
  }

  return true;
}

int main(int argc, char *argv[]) {

  bool print_color = true;
  bool print_full_meta = false;

  if (argc < 3) {
    printf("Please provide more than 1 file to compare, or use dcm-dump to "
           "print only one file.\n");
    printf("%s\n", usage);
  }

  struct winsize ws;
  if (get_winsize(&ws) < 0) {
    printf("Unable to create winsize\n");
    return EXIT_FAILURE;
  }
  int cols_per_section = ws.ws_col / 2; // truncation okay

  int c;
  while ((c = dcm_getopt(argc, argv, "h?Vviw")) != -1) {
    switch (c) {
    case 'h':
    case '?':
      printf("%s\n", usage);
      return EXIT_SUCCESS;

    case 'V':
      printf("%s\n", dcm_get_version());
      return EXIT_SUCCESS;

    case 'v':
    case 'i':
      dcm_log_set_level(DCM_LOG_INFO);
      break;

    case 'w':
      dcm_log_set_level(DCM_LOG_WARNING);
      break;
    case 'c':
      print_color = false;
      break;
    case 'l':
      print_full_meta = true;
      break;

    case '#':
    default:
      return EXIT_FAILURE;
    }
  }
  DcmError *error_file1 = NULL;
  DcmFilehandle *filehandle_1 = NULL;
  DcmError *error_file2 = NULL;
  DcmFilehandle *filehandle_2 = NULL;
  if (read_dicom_and_error(argv[dcm_optind], &error_file1, &filehandle_1) !=
          0 ||
      read_dicom_and_error(argv[dcm_optind + 1], &error_file2, &filehandle_2) !=
          0) {
    dcm_error_get_message(error_file1);
    dcm_error_get_summary(error_file1);
    dcm_error_get_message(error_file2);
    dcm_error_get_summary(error_file2);
    dcm_error_log(error_file1);
    dcm_error_clear(&error_file1);
    dcm_error_log(error_file2);
    dcm_error_clear(&error_file2);
    return EXIT_FAILURE;
  }
  const DcmDataSet *metadata_1 =
      dcm_filehandle_read_metadata(&error_file1, filehandle_1, NULL);
  const DcmDataSet *metadata_2 =
      dcm_filehandle_read_metadata(&error_file2, filehandle_2, NULL);
  if (metadata_1 == NULL || metadata_2 == NULL) {
    dcm_error_get_message(error_file1);
    dcm_error_get_summary(error_file1);
    dcm_error_get_message(error_file2);
    dcm_error_get_summary(error_file2);
    dcm_error_log(error_file1);
    dcm_error_clear(&error_file1);
    dcm_error_log(error_file2);
    dcm_error_clear(&error_file2);
    cleanup_fhandles(filehandle_1, filehandle_2);
    return EXIT_FAILURE;
  }
  printf("foreach\n");

  bool d1 = dcm_dataset_foreach(metadata_1, *print_metadata_element,
                                (void *)metadata_2);

  cleanup_fhandles(filehandle_1, filehandle_2);
  return EXIT_SUCCESS;
}
