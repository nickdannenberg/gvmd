/* OpenVAS Manager
 * $Id$
 * Description: LSC user credentials package generation.
 *
 * Authors:
 * Matthew Mundell <matthew.mundell@intevation.de>
 * Michael Wiegand <michael.wiegand@intevation.de>
 * Felix Wolfsteller <felix.wolfsteller@intevation.de>
 *
 * Copyright:
 * Copyright (C) 2009 Greenbone Networks GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2,
 * or, at your option, any later version as published by the Free
 * Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <glib/gstdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <openvas/openvas_ssh_login.h>
#include <openvas/system.h>

#undef G_LOG_DOMAIN
/**
 * @brief GLib log domain.
 */
#define G_LOG_DOMAIN "md manage"


/* Helpers. */

/** @todo Copied check_is_file and check_is_dir from administrator. */

/**
 * @brief Checks whether a file is a directory or not.
 *
 * This is a replacement for the g_file_test functionality which is reported
 * to be unreliable under certain circumstances, for example if this
 * application and glib are compiled with a different libc.
 *
 * @todo FIXME: handle symbolic links
 * @todo Move to libs?
 *
 * @param[in]  name  File name.
 *
 * @return 1 if parameter is directory, 0 if it is not, -1 if it does not
 *         exist or could not be accessed.
 */
static int
check_is_file (const char *name)
{
  struct stat sb;

  if (stat (name, &sb))
    {
      return -1;
    }
  else
    {
      return (S_ISREG (sb.st_mode));
    }
}

/**
 * @brief Checks whether a file is a directory or not.
 *
 * This is a replacement for the g_file_test functionality which is reported
 * to be unreliable under certain circumstances, for example if this
 * application and glib are compiled with a different libc.
 *
 * @todo FIXME: handle symbolic links
 * @todo Move to libs?
 *
 * @param[in]  name  File name.
 *
 * @return 1 if parameter is directory, 0 if it is not, -1 if it does not
 *         exist or could not be accessed.
 */
static int
check_is_dir (const char *name)
{
  struct stat sb;

  if (stat (name, &sb))
    {
      return -1;
    }
  else
    {
      return (S_ISDIR (sb.st_mode));
    }
}


/* Modified copy of openvas-client/src/util/file_utils.c. */

/**
 * @brief Recursively removes files and directories.
 *
 * This function will recursively call itself to delete a path and any
 * contents of this path.
 *
 * @param[in]  pathname  Name of file to be deleted from filesystem.
 *
 * @return 0 if the name was successfully deleted, -1 if an error occurred.
 */
static int
file_utils_rmdir_rf (const gchar * pathname)
{
  if (check_is_dir (pathname) == 1)
    {
      GError *error = NULL;
      GDir *directory = g_dir_open (pathname, 0, &error);

      if (directory == NULL)
        {
          g_warning ("g_dir_open(%s) failed - %s\n", pathname, error->message);
          g_error_free (error);
          return -1;
        }
      else
        {
          int ret = 0;
          const gchar *entry = NULL;

          while ((entry = g_dir_read_name (directory)) && (ret == 0))
            {
              ret = file_utils_rmdir_rf (g_build_filename (pathname,
                                                           entry,
                                                           NULL));
              if (ret != 0)
                {
                  g_warning ("Failed to remove %s from %s!", entry, pathname);
                  g_dir_close (directory);
                  return ret;
                }
            }
          g_dir_close (directory);
        }
    }

  return g_remove (pathname);
}

/**
 * @brief Reads contents from a source file into a destination file.
 *
 * The source file is read into memory, so it is inefficient and likely to fail
 * for really big files.
 *
 * If the destination file does exist already, it will be overwritten.
 *
 * @param[in]  source_file  Source file name.
 * @param[in]  dest_file    Destination file name.
 *
 * @return TRUE if successful, FALSE otherwise.
 */
static gboolean
file_utils_copy_file (const gchar *source_file, const gchar *dest_file)
{
  gchar *src_file_content = NULL;
  gsize src_file_size = 0;
  int bytes_written = 0;
  FILE *fd = NULL;
  GError *error;

  /* Read file content into memory. */

  error = NULL;
  g_file_get_contents (source_file, &src_file_content, &src_file_size, &error);
  if (error)
    {
      g_debug ("%s: failed to read %s: %s",
               __FUNCTION__, source_file, error->message);
      g_error_free (error);
      return FALSE;
    }

  /* Open destination file. */

  fd = fopen (dest_file, "wb");
  if (fd == NULL)
    {
      g_debug ("%s: failed to open %s", __FUNCTION__, dest_file);
      g_free (src_file_content);
      return FALSE;
    }

  /* Write content of src to dst and close it. */

  bytes_written = fwrite (src_file_content, 1, src_file_size, fd);
  fclose (fd);

  if (bytes_written != src_file_size)
    {
      g_debug ("%s: failed to write to %s (%d/%d)",
               __FUNCTION__, dest_file, bytes_written, src_file_size);
      g_free (src_file_content);
      return FALSE;
    }
  g_free (src_file_content);

  return TRUE;
}

/**
 * @brief Reads contents from a source file into a destination file
 * @brief and unlinks the source file.
 *
 * The source file is read into memory, so it is inefficient and likely to fail
 * for really big files.
 *
 * If the destination file does exist already, it will be overwritten.
 *
 * @param[in]  source_file  Source file name.
 * @param[in]  dest_file    Destination file name.
 *
 * @return TRUE if successful, FALSE otherwise (displays error but does not
 *         clean up).
 */
static gboolean
file_utils_move_file (const gchar *source_file, const gchar *dest_file)
{
  /* Copy file (will displays errors itself). */

  if (file_utils_copy_file (source_file, dest_file) == FALSE)
    return FALSE;

  /* Remove source file. */

  if (remove (source_file) != 0)
    {
      g_debug ("%s: failed to remove %s", __FUNCTION__, source_file);
      return FALSE;
    }

  return TRUE;
}


/* Key creation. */

/**
 * @brief Create a private key for local checks.
 *
 * Forks and creates a key for local checks by calling
 * "openssl pkcs8 -topk8 -v2 des3 -in filepath -passin pass:passphrase -out
 *          filepath.p8 -passout pass:passphrase"
 * Directories within privkey_file will be created if they do not exist.
 *
 * @param[in]  pubkey_file      Path to file of public key (a .pub will be
 *                              stripped).
 * @param[in]  privkey_file     Name of private key file to be created.
 * @param[in]  passphrase_pub   The passphrase for the public key.
 * @param[in]  passphrase_priv  Passhprase for the private key.
 *
 * @return 0 if successful, -1 otherwise.
 */
static int
ssh_privkey_create (char *pubkey_file, char *privkey_file,
                    char *passphrase_pub, char *passphrase_priv)
{
  gchar *astdout = NULL;
  gchar *astderr = NULL;
  GError *err = NULL;
  gint exit_status;
  gchar *dir = NULL;
  gchar *pubkey_stripped = NULL;

  /* Sanity-check essential parameters. */
  if (!passphrase_pub || !passphrase_priv)
    {
      g_debug ("%s: parameter error", __FUNCTION__);
      return -1;
    }

  /* Sanity check files. */
  if (g_file_test (pubkey_file, G_FILE_TEST_EXISTS) == FALSE)
    {
      g_debug ("%s: failed to find public key %s", __FUNCTION__, pubkey_file);
      return -1;
    }
  if (g_file_test (privkey_file, G_FILE_TEST_EXISTS))
    {
      g_debug ("%s: file already exists.", __FUNCTION__);
      return -1;
    }
  dir = g_path_get_dirname (privkey_file);
  if (g_mkdir_with_parents (dir, 0755 /* "rwxr-xr-x" */ ))
    {
      g_debug ("%s: failed to access dir folder %s", __FUNCTION__, dir);
      g_free (dir);
      return -1;
    }
  g_free (dir);

  /* Strip ".pub" of public key filename, if any. */

  if (g_str_has_suffix (pubkey_file, ".pub") == TRUE)
    {
      /* RATS: ignore, string literal is nul-terminated */
      pubkey_stripped = g_malloc (strlen (pubkey_file) - strlen (".pub") + 1);
      g_strlcpy (pubkey_stripped,
                 pubkey_file,
                 strlen (pubkey_file) - strlen (".pub") + 1);
    }
  else
    pubkey_stripped = g_strdup (pubkey_file);

  /* Spawn openssl. */

  const gchar *command =
    g_strconcat ("openssl pkcs8 -topk8 -v2 des3"
                 " -in ", pubkey_stripped,
                 " -passin pass:", passphrase_pub,
                 " -out ", privkey_file,
                 " -passout pass:", passphrase_priv,
                 NULL);
  g_free (pubkey_stripped);

  g_debug ("command: %s", command);

  if ((g_spawn_command_line_sync (command, &astdout, &astderr, &exit_status,
                                  &err)
       == FALSE)
      || (WIFEXITED (exit_status) == 0)
      || WEXITSTATUS (exit_status))
    {
      g_debug ("%s: openssl failed with %d", __FUNCTION__, exit_status);
      g_debug ("%s: stdout: %s", __FUNCTION__, astdout);
      g_debug ("%s: stderr: %s", __FUNCTION__, astderr);
      return -1;
    }

  return 0;
}

/**
 * @brief Create a public key.
 *
 * Forks and creates a key for local checks by calling
 * "ssh-keygen -t rsa -f filepath -C comment -P passhprase -q".
 * A directory will be created if it does not exist.
 *
 * @param[in]  comment     Comment to use.
 * @param[in]  passphrase  Passphrase for key, must be longer than 4 characters.
 * @param[in]  filepath    Path to file of public key (a .pub will be stripped).
 *
 * @return 0 if successful, -1 otherwise.
 */
static int
ssh_pubkey_create (const char *comment, char *passphrase, char *filepath)
{
  gchar *astdout = NULL;
  gchar *astderr = NULL;
  GError *err = NULL;
  gint exit_status = 0;
  gchar *dir;
  gchar *file_pubstripped;
  const char *command;

  /* Sanity-check essential parameters. */

  if (!comment || comment[0] == '\0')
    {
      g_debug ("%s: comment must be set", __FUNCTION__);
      return -1;
    }
  if (!passphrase || strlen (passphrase) < 5)
    {
      g_debug ("%s: password must be longer than 4 characters", __FUNCTION__);
      return -1;
    }

  /* Sanity check files. */

  dir = g_path_get_dirname (filepath);
  if (g_mkdir_with_parents (dir, 0755 /* "rwxr-xr-x" */ ))
    {
      g_debug ("%s: failed to access %s", __FUNCTION__, dir);
      g_free (dir);
      return -1;
    }
  g_free (dir);

  /* Strip ".pub" off filename, if any. */

  if (g_str_has_suffix (filepath, ".pub") == TRUE)
    {
      /* RATS: ignore, string literal is nul-terminated */
      file_pubstripped = g_malloc (strlen (filepath) - strlen (".pub") + 1);
      g_strlcpy (file_pubstripped,
                 filepath,
                 strlen (filepath) - strlen (".pub") + 1);
    }
  else
    file_pubstripped = g_strdup (filepath);

  /* Spawn ssh-keygen. */

  command = g_strconcat ("ssh-keygen -t rsa"
                         " -f ", file_pubstripped,
                         " -C \"", comment, "\""
                         " -P \"", passphrase, "\"",
                         NULL);
  g_free (file_pubstripped);

  g_debug ("command: %s", command);

  if ((g_spawn_command_line_sync (command, &astdout, &astderr, &exit_status,
                                  &err)
       == FALSE)
      || (WIFEXITED (exit_status) == 0)
      || WEXITSTATUS (exit_status))
    {
      if (err)
        {
          g_debug ("%s: failed to create public key: %s\n",
                   __FUNCTION__, err->message);
          g_error_free (err);
        }
      else
        g_debug ("%s: failed to create public key\n", __FUNCTION__);
      g_debug ("\tSpawned key-gen process returned with %d (WIF %i, WEX %i).\n",
               exit_status, WIFEXITED (exit_status), WEXITSTATUS (exit_status));
      g_debug ("\t\t stdout: %s", astdout);
      g_debug ("\t\t stderr: %s", astderr);
      return -1;
    }
  return 0;
}


/* RPM package generation. */

/**
 * @brief Return directory containing rpm generator script.
 *
 * The search will be performed just once.
 *
 * @return Newly allocated path to directory containing generator if found,
 *         else NULL.
 */
static gchar *
get_rpm_generator_path ()
{
  static gchar *rpm_generator_path = NULL;

  if (rpm_generator_path == NULL)
    {
      gchar *path_exec = g_build_filename (OPENVAS_DATA_DIR,
                                           "openvas-lsc-rpm-creator.sh",
                                           NULL);
      if (check_is_file (path_exec) == 0)
        {
          g_free (path_exec);
          return NULL;
        }
      g_free (path_exec);
      rpm_generator_path = g_strdup (OPENVAS_DATA_DIR);
    }

  return rpm_generator_path;
}

/**
 * @brief Attempts creation of RPM packages to install a users public key file.
 *
 * @param[in]  loginfo  openvas_ssh_login struct to create rpm for.
 *
 * @return Path to rpm file if successfull, NULL otherwise.
 */
static gboolean
lsc_user_rpm_create (openvas_ssh_login *loginfo, const gchar *to_filename)
{
  gchar *oltap_path;
  gchar *rpm_path = NULL;
  gint exit_status;
  gchar *new_pubkey_filename = NULL;
  gchar *pubkey_basename = NULL;
  gchar **cmd;
  char tmpdir[] = "/tmp/lsc_user_rpm_create_XXXXXX";
  gboolean success = TRUE;

  oltap_path = get_rpm_generator_path ();

  /* Create a temporary directory. */

  g_debug ("%s: create temporary directory", __FUNCTION__);
  if (mkdtemp (tmpdir) == NULL)
    return FALSE;
  g_debug ("%s: temporary directory: %s\n", __FUNCTION__, tmpdir);

  /* Copy the public key into the temporary directory. */

  g_debug ("%s: copy key to temporary directory\n", __FUNCTION__);
  pubkey_basename = g_strdup_printf ("%s.pub", loginfo->username);
  new_pubkey_filename = g_build_filename (tmpdir, pubkey_basename, NULL);
  if (file_utils_copy_file (loginfo->public_key_path, new_pubkey_filename)
      == FALSE)
    {
      g_debug ("%s: failed to copy key file %s to %s",
               __FUNCTION__, loginfo->public_key_path, new_pubkey_filename);
      g_free (pubkey_basename);
      g_free (new_pubkey_filename);
      return FALSE;
    }

  /* Execute create-rpm script with the temporary directory as the
   * target and the public key in the temporary directory as the key. */

  g_debug ("%s: Attempting RPM build\n", __FUNCTION__);
  cmd = (gchar **) g_malloc (5 * sizeof (gchar *));
  cmd[0] = g_strdup ("./openvas-lsc-rpm-creator.sh");
  cmd[1] = g_strdup ("--target");
  cmd[2] = g_strdup (tmpdir);
  cmd[3] = g_build_filename (tmpdir, pubkey_basename, NULL);
  cmd[4] = NULL;
  g_debug ("%s: Spawning in %s: %s %s %s %s\n",
           __FUNCTION__, oltap_path, cmd[0], cmd[1], cmd[2], cmd[3]);
  gchar *standard_out;
  gchar *standard_err;
  if ((g_spawn_sync (oltap_path,
                     cmd,
                     NULL,                  /* Environment. */
                     G_SPAWN_SEARCH_PATH,
                     NULL,                  /* Setup function. */
                     NULL,
                     &standard_out,
                     &standard_err,
                     &exit_status,
                     NULL)
       == FALSE)
      || exit_status)
    {
      g_debug ("%s: failed to creating the rpm: %d", __FUNCTION__, exit_status);
      g_debug ("%s: sout: %s\n", __FUNCTION__, standard_out);
      g_debug ("%s: serr: %s\n", __FUNCTION__, standard_err);
      success = FALSE;
    }

  g_free (cmd[0]);
  g_free (cmd[1]);
  g_free (cmd[2]);
  g_free (cmd[3]);
  g_free (cmd[4]);
  g_free (cmd);
  g_free (pubkey_basename);
  g_free (new_pubkey_filename);
  g_debug ("%s: cmd returned %d.\n", __FUNCTION__, exit_status);

  /* Build the filename that the RPM in the temporary directory has,
   * for example RPMS/noarch/openvas-lsc-target-example_user-0.5-1.noarch.rpm.
   */

  gchar *rpmfile = g_strconcat ("openvas-lsc-target-",
                                loginfo->username,
                                "-0.5-1.noarch.rpm",
                                NULL);
  rpm_path = g_build_filename (tmpdir, rpmfile, NULL);
  g_debug ("%s: new filename (rpm_path): %s\n", __FUNCTION__, rpm_path);

  /* Move the RPM from the temporary directory to the given destination. */

  if (file_utils_move_file (rpm_path, to_filename) == FALSE && success == TRUE)
    {
      g_debug ("%s: failed to move RPM %s to %s",
               __FUNCTION__, rpm_path, to_filename);
      success = FALSE;
    }

  /* Remove the copy of the public key and the temporary directory. */

  if (file_utils_rmdir_rf (tmpdir) != 0 && success == TRUE)
    {
      g_debug ("%s: failed to remove temporary directory %s",
               __FUNCTION__, tmpdir);
      success = FALSE;
    }

  g_free (rpm_path);
  g_free (rpmfile);

  return success;
}


/* Deb generation. */

/**
 * @brief Execute alien to create a deb package from an rpm package.
 *
 * @param[in]  rpmdir   Directory to run the command in.
 * @param[in]  rpmfile  .rpm file to transform with alien to a .deb.
 *
 * @return 0 success, -1 error.
 */
static int
execute_alien (const gchar *rpmdir, const gchar *rpmfile)
{
  gchar **cmd;
  gint exit_status = 0;

  cmd = (gchar **) g_malloc (7 * sizeof (gchar *));

  cmd[0] = g_strdup ("fakeroot");
  cmd[1] = g_strdup ("--");
  cmd[2] = g_strdup ("alien");
  cmd[3] = g_strdup ("--scripts");
  cmd[4] = g_strdup ("--keep-version");
  cmd[5] = g_strdup (rpmfile);
  cmd[6] = NULL;
  g_debug ("--- executing alien.\n");
  g_debug ("%s: Spawning in %s: %s %s %s %s %s %s\n",
           __FUNCTION__,
           rpmdir, cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5]);
  if ((g_spawn_sync (rpmdir,
                     cmd,
                     NULL,                 /* Environment. */
                     G_SPAWN_SEARCH_PATH,
                     NULL,                 /* Setup func. */
                     NULL,
                     NULL,
                     NULL,
                     &exit_status,
                     NULL) == FALSE)
      || exit_status)
    {
      exit_status = -1;
    }

  g_free (cmd[0]);
  g_free (cmd[1]);
  g_free (cmd[2]);
  g_free (cmd[3]);
  g_free (cmd[4]);
  g_free (cmd[5]);
  g_free (cmd[6]);
  g_free (cmd);

  g_debug ("--- alien returned %d.\n", exit_status);
  return exit_status;
}

/**
 * @brief Create a deb packages from an rpm package.
 *
 * @param[in]  loginfo   openvas_ssh_login struct to create rpm for.
 * @param[in]  rpm_file  location of the rpm file.
 *
 * @return deb package file name on success, else NULL.
 */
gchar *
lsc_user_deb_create (openvas_ssh_login *loginfo, const gchar *rpm_file)
{
  gchar *dirname = g_path_get_dirname (rpm_file);
  gchar *dir = g_strconcat (dirname, "/", NULL);
  gchar *basename = g_path_get_basename (rpm_file);
  gchar *username = g_strdup (loginfo->username ? loginfo->username : "user");
  gchar *deb_name = g_strdup_printf ("%s/openvas-lsc-target-%s_0.5-1_all.deb",
                                     dirname,
                                     g_ascii_strdown (username, -1));

  g_free (dirname);
  g_free (username);

  if (execute_alien (dir, basename))
    {
      g_free (dir);
      g_free (basename);
      g_free (deb_name);
      return NULL;
    }

  g_free (dir);
  g_free (basename);

  return deb_name;
}

/**
 * @brief Returns whether alien could be found in the path.
 *
 * The check itself will only be done once.
 *
 * @return TRUE if alien could be found in the path, FALSE otherwise.
 */
static gboolean
alien_found ()
{
  static gboolean searched = FALSE;
  static gboolean found = FALSE;

  if (searched == FALSE)
    {
      /* Check if alien is found in path. */
      gchar *alien_path = g_find_program_in_path ("alien");
      if (alien_path != NULL)
        {
          found = TRUE;
          g_free (alien_path);
        }
      searched = TRUE;
    }

  return found;
}


/* Generation of all packages. */

/**
 * @brief Create local security check (LSC) packages.
 *
 * @param[in]   name         User name.
 * @param[in]   password     Password.
 * @param[out]  public_key   Public key.
 * @param[out]  private_key  Private key.
 * @param[out]  rpm          RPM package.
 * @param[out]  rpm_size     Size of RPM package, in bytes.
 * @param[out]  deb          Debian package.
 * @param[out]  deb_size     Size of Debian package, in bytes.
 * @param[out]  exe          NSIS package.
 * @param[out]  exe_size     Size of NSIS package, in bytes.
 *
 * @return 0 success, -1 error.
 */
int
lsc_user_all_create (const gchar *name,
                     const gchar *password,
                     gchar **public_key,
                     gchar **private_key,
                     void **rpm, gsize *rpm_size,
                     void **deb, gsize *deb_size,
                     void **exe, gsize *exe_size)
{
  GError *error;
  gsize length;
  char *key_name, *comment, *key_password, *public_key_path;
  char *private_key_path, *user_name, *user_password;
  char rpm_dir[] = "/tmp/rpm_XXXXXX";
  char key_dir[] = "/tmp/key_XXXXXX";
  gchar *rpm_path, *deb_path;
  int ret = -1;

  if (alien_found () == FALSE)
    return -1;

  /* Make a directory for the keys. */

  if (mkdtemp (key_dir) == NULL)
    return -1;

  /* Setup the login structure. */

  /* These are freed by openvas_ssh_login_free with efree. */
  // FIX emalloc
  public_key_path = g_build_filename (key_dir, "key.pub", NULL);
  private_key_path = g_build_filename (key_dir, "key.priv", NULL);
  key_name = estrdup ("key_name");
  comment = estrdup ("Key generated by OpenVAS Manager");
  key_password = estrdup (password);
  user_name = estrdup (name);
  user_password = estrdup (password);

  openvas_ssh_login *login = openvas_ssh_login_new (key_name,
                                                    public_key_path,
                                                    private_key_path,
                                                    key_password,
                                                    comment,
                                                    user_name,
                                                    user_password);

  /* Create public key. */

  if (ssh_pubkey_create (comment, key_password, public_key_path))
    goto rm_key_exit;

  /* Create private key. */

  if (ssh_privkey_create (public_key_path,
                          private_key_path,
                          key_password,
                          key_password))
    goto rm_key_exit;

  /* Create RPM package. */

  if (mkdtemp (rpm_dir) == NULL)
    goto rm_key_exit;
  rpm_path = g_build_filename (rpm_dir, "p.rpm", NULL);
  g_debug ("%s: rpm_path: %s", __FUNCTION__, rpm_path);
  if (lsc_user_rpm_create (login, rpm_path) == FALSE)
    {
      g_free (rpm_path);
      goto rm_exit;
    }

  /* Create Debian package. */

  deb_path = lsc_user_deb_create (login, rpm_path);
  if (deb_path == NULL)
    {
      g_free (rpm_path);
      g_free (deb_path);
      goto rm_exit;
    }
  g_debug ("%s: deb_path: %s", __FUNCTION__, deb_path);

#if 0
  /** @todo Create NSIS installer. */

  exe_path = lsc_user_exe_create (login);
  if (exe_path == NULL)
    {
      g_free (rpm_path);
      g_free (deb_path);
      g_free (exe_path);
      goto rm_exit;
    }
  g_debug ("%s: exe_path: %s", __FUNCTION__, deb_path);
#endif

  /* Read the packages and key into memory. */

  error = NULL;
  g_file_get_contents (public_key_path, public_key, &length, &error);
  if (error)
    {
      g_free (rpm_path);
      g_free (deb_path);
      g_error_free (error);
      goto rm_exit;
    }

  error = NULL;
  g_file_get_contents (private_key_path, private_key, &length, &error);
  if (error)
    {
      g_free (rpm_path);
      g_free (deb_path);
      g_error_free (error);
      goto rm_exit;
    }

  error = NULL;
  g_file_get_contents (rpm_path, (gchar **) rpm, rpm_size, &error);
  g_free (rpm_path);
  if (error)
    {
      g_error_free (error);
      g_free (deb_path);
      goto rm_exit;
    }

  error = NULL;
  g_file_get_contents (deb_path, (gchar **) deb, deb_size, &error);
  g_free (deb_path);
  if (error)
    {
      g_error_free (error);
      goto rm_exit;
    }

  *exe = g_strdup ("");
  *exe_size = 0;

  /* Return. */

  ret = 0;

 rm_exit:

  file_utils_rmdir_rf (rpm_dir);

 rm_key_exit:

  file_utils_rmdir_rf (key_dir);

  openvas_ssh_login_free (login);

  return ret;
}
