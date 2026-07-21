<?php
$value = '';
for ($i = 0; $i < 10000; $i++) {
    $value .= '0123456789abcdef';
    if (strlen($value) > 1024) $value = '';
}
echo strlen($value);
