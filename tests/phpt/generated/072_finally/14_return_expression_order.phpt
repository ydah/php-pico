--TEST--
return expression evaluates before finally
--FILE--
<?php
function final_mark($label) { echo $label; return $label; }
function final_expression_order() {
    try { return final_mark('A'); } finally { final_mark('B'); }
}
$result = final_expression_order(); echo ':', $result;
--EXPECT--
AB:A
