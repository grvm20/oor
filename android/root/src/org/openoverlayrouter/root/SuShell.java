/*
 *
 * Copyright (C) 2011, 2015 Cisco Systems, Inc.
 * Copyright (C) 2015 CBA research group, Technical University of Catalonia.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

package org.openoverlayrouter.root;

import java.io.BufferedReader;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.List;

public class SuShell {
	
	private Process 			process;
	private DataOutputStream 	stdin;
	private BufferedReader 		stdout;
	private BufferedReader 		stderr;
	
	
	public SuShell() throws IOException 
	{
		process = Runtime.getRuntime().exec("su");
		stdin  = new DataOutputStream(process.getOutputStream());
		stdout = new BufferedReader(new InputStreamReader(process.getInputStream()));
		stderr = new BufferedReader(new InputStreamReader(process.getErrorStream()));
	}
	
	public String run(String command)
	{
		String res 	= "";
		try {
			stdin.writeBytes(command+"\n");
			stdin.flush();
			if (stdout.ready()) {
				res = stdout.readLine();
			}
			if (stderr.ready()) {
				res = stdout.readLine();
			}
			createLogFile(res);
		} catch (IOException e) {
			StringWriter errors = new StringWriter();
			e.printStackTrace(new PrintWriter(errors));
			createLogFile(errors.toString());
		}
		return res;
	}
	
	public void run_no_output(String command)
	{
		try {
			stdin.writeBytes(command+"\n");
			stdin.flush();
		
		} catch (IOException e) {
			e.printStackTrace();
		}
	}

	public void createLogFile(String message)
	{
		/*
		 * If a configuration file is not found, a default configuration file is created.
		 * */
		try {

			FileWriter fstream = new FileWriter(log_file);
			BufferedWriter out = new BufferedWriter(fstream);
			out.append(message);
			out.close();

		} catch (Exception e) {
			//displayMessage("Error while writing Default Conf file to sdcard!!", false, null);
		}

	}

}
