--TEST--
finally executes after return from catch
--FILE--
<?php
function final_catch_return() {
    try { throw new Exception('x'); }
    catch (Exception $error) { return 'catch'; }
    finally { echo 'finally:'; }
}
echo final_catch_return();
--EXPECT--
finally:catch
